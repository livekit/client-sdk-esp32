import os
import uuid

import pytest
from dotenv import load_dotenv
from livekit import api
from pytest_embedded_idf.utils import idf_parametrize

load_dotenv()  # Shell variables take precedence

TARGETS = ["esp32s3", "esp32p4"]


@idf_parametrize("target", TARGETS, indirect=["target"])
def test_basic(dut):
    dut.run_all_single_board_cases(group="basic")


@idf_parametrize("target", TARGETS, indirect=["target"])
@pytest.mark.skipif(not os.getenv("LIVEKIT_URL"), reason="LIVEKIT_URL not set")
def test_connect(dut):
    token = (
        api.AccessToken()  # Reads LIVEKIT_API_KEY and LIVEKIT_API_SECRET
        .with_identity("esp32-test")
        .with_grants(api.VideoGrants(room_join=True, room=f"test_room_{uuid.uuid4().hex}"))
        .to_jwt()
    )
    params = {
        "WIFI_SSID": os.getenv("WIFI_SSID", ""),  # Ignored on Ethernet boards
        "WIFI_PASSWORD": os.getenv("WIFI_PASSWORD", ""),
        "LIVEKIT_URL": os.environ["LIVEKIT_URL"],
        "TOKEN": token,
    }
    dut.expect_exact("Press ENTER to see the list of tests")
    # Resends if the board drops the input while starting up
    dut.confirm_write('"connect"', expect_str="Running connect...")
    for name, value in params.items():
        dut.expect_exact(f"Enter {name}")
        dut.write(value)
    dut.expect_unity_test_output(timeout=60)
