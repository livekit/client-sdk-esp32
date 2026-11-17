from pytest_embedded_idf.utils import idf_parametrize

TARGETS = ["esp32s3", "esp32p4"]


@idf_parametrize("target", TARGETS, indirect=["target"])
def test_basic(dut):
    dut.run_all_single_board_cases(group="basic")
