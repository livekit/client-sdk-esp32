import json
import logging
import os
import textwrap
from enum import Enum

from dotenv import load_dotenv
from livekit.agents import (
    Agent,
    AgentServer,
    AgentSession,
    JobContext,
    RunContext,
    ToolError,
    TurnHandlingOptions,
    cli,
    function_tool,
    inference,
    mock_tools,
    room_io,
)
from livekit.plugins import noise_cancellation

load_dotenv(".env.local")


class LEDColor(str, Enum):
    RED = "red"
    GREEN = "green"
    BLUE = "blue"
    WHITE = "white"


async def call_board(context: RunContext, method: str, payload: str = "") -> str:
    """Invoke an RPC method registered by the ESP32 board."""
    room_io = context.session.room_io
    board = room_io.linked_participant
    if board is None:
        raise RuntimeError("no participant linked to the session")
    return await room_io.room.local_participant.perform_rpc(
        destination_identity=board.identity,
        method=method,
        payload=payload,
        response_timeout=10,
    )


class Assistant(Agent):
    def __init__(self, board_info: dict) -> None:
        exposed_controls = board_info.get("exposed_controls", [])
        if "RGB LED" in exposed_controls:
            led_guidance = (
                "The board has one RGB pixel. Setting a color replaces its "
                "previous color, and turning it off clears that pixel. "
                "To turn on or change its color, call set_led_state exactly once "
                "with the requested color and state=true. Never send state=false "
                "for other colors: those calls turn off the entire pixel. "
                "Leave the pixel on until the user explicitly requests off. "
                "For an off request, call set_led_state exactly once with "
                "state=false. Do not blink or cycle colors unless requested."
            )
        elif "red LED" in exposed_controls or "blue LED" in exposed_controls:
            led_guidance = (
                "The board exposes independent red and blue LEDs; green and "
                "white are unsupported."
            )
        else:
            led_guidance = "This example exposes no LED control on this board."
        super().__init__(
            instructions=textwrap.dedent(
                f"""\
                You are a helpful voice AI assistant connected to this hardware:
                {json.dumps(board_info)}
                Answer questions using only these reported capabilities. Distinguish hardware that is physically
                present from controls exposed by this LiveKit example. Never claim that an unexposed peripheral can
                be controlled. {led_guidance} You cannot read the current LED state. No markdown is allowed in your
                responses.
                """
            )
        )

    async def on_enter(self) -> None:
        self.session.generate_reply(
            instructions="Greet the user and briefly say what you can do with the board."
        )

    @function_tool()
    async def set_led_state(
        self, context: RunContext, led: LEDColor, state: bool
    ) -> None:
        """Set the state of an on-board LED.

        Args:
            led: Which LED to set. S3 supports red and blue only; S31 RGB
                boards also support green and white.
            state: The state to set the LED to (i.e. on or off).
        """
        try:
            await call_board(
                context,
                "set_led_state",
                json.dumps({"color": led.value, "state": state}),
            )
        except Exception:
            raise ToolError("Unable to set LED state") from None

    @function_tool()
    async def get_cpu_temp(self, context: RunContext) -> float:
        """Get the current temperature of the CPU.

        Returns:
            The temperature reading in degrees Celsius.
        """
        try:
            return float(await call_board(context, "get_cpu_temp"))
        except Exception:
            raise ToolError("Unable to retrieve CPU temperature") from None


async def get_board_info(ctx: JobContext, participant_identity: str) -> dict:
    try:
        response = await ctx.room.local_participant.perform_rpc(
            destination_identity=participant_identity,
            method="get_board_info",
            response_timeout=10,
            payload="",
        )
        info = json.loads(response)
        if isinstance(info, dict):
            return info
    except Exception:
        logging.getLogger("esp32-agent").debug(
            "Board discovery unavailable", exc_info=True
        )
    return {
        "board": "ESP32-S3-Korvo-2 or firmware without board discovery",
        "led": "independent red and blue indicator LEDs",
        "exposed_controls": ["red LED", "blue LED", "CPU temperature"],
    }


server = AgentServer()


@server.rtc_session()
async def entrypoint(ctx: JobContext):
    ctx.log_context_fields = {"room": ctx.room.name}

    # STT-LLM-TTS pipeline served by LiveKit Inference; no provider API keys needed.
    # See https://docs.livekit.io/agents/models/ for available models.
    session = AgentSession(
        stt=inference.STT(model="assemblyai/universal-3-5-pro", language="en"),
        llm=inference.LLM(model="google/gemma-4-31b-it"),
        tts=inference.TTS(
            model="fishaudio/s2.1-pro",
            voice=os.getenv(
                "LIVEKIT_AGENT_VOICE_ID", "fa4c9eb3dccc4806b382b40d61c6b10a"
            ),
        ),
        turn_handling=TurnHandlingOptions(
            turn_detection=inference.TurnDetector(),
            interruption={"mode": "adaptive"},
            preemptive_generation={"enabled": True},
        ),
        expressive=True,
    )
    participant_identity = None
    if ctx.is_fake_job():
        board_info = {
            "board": "Console simulation (no physical board connected)",
            "exposed_controls": ["red LED", "blue LED", "CPU temperature"],
        }
        # Console mode has no board to call, so stub the hardware tools.
        mock_tools(
            Assistant,
            {"get_cpu_temp": lambda: 25.0, "set_led_state": lambda: None},
            session=session,
        )
    else:
        await ctx.connect()
        participant = await ctx.wait_for_participant()
        participant_identity = participant.identity
        board_info = await get_board_info(ctx, participant_identity)
        logging.getLogger("esp32-agent").info(
            "Detected board: %s", board_info.get("board")
        )

    await session.start(
        room=ctx.room,
        agent=Assistant(board_info),
        room_options=room_io.RoomOptions(
            participant_identity=participant_identity,
            audio_input=room_io.AudioInputOptions(
                noise_cancellation=noise_cancellation.BVC(),
            ),
            # The board renders no text, so skip streaming transcriptions to it.
            text_output=False,
        ),
    )


if __name__ == "__main__":
    cli.run_app(server)
