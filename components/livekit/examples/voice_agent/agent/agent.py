import json
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
    BLUE = "blue"


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
    def __init__(self) -> None:
        super().__init__(
            instructions=textwrap.dedent(
                """\
                You are a helpful voice AI assistant running on an ESP32 dev board.
                You answer user's questions about the hardware state and control the hardware based on their requests.
                The board has discrete LEDs that can be controlled independently. Each LED has a static color
                that cannot be changed. While you are able to set the state of the LEDs, you are not able to read the
                state which could be changed without your knowledge. No markdown is allowed in your responses.
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
            led: Which LED to set the state of.
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
            model="fishaudio/s2.1-pro", voice="fa4c9eb3dccc4806b382b40d61c6b10a"
        ),
        turn_handling=TurnHandlingOptions(
            turn_detection=inference.TurnDetector(),
            interruption={"mode": "adaptive"},
            preemptive_generation={"enabled": True},
        ),
        expressive=True,
    )
    if ctx.is_fake_job():
        # Console mode has no board to call, so stub the hardware tools.
        mock_tools(
            Assistant,
            {"get_cpu_temp": lambda: 25.0, "set_led_state": lambda: None},
            session=session,
        )

    await session.start(
        room=ctx.room,
        agent=Assistant(),
        room_options=room_io.RoomOptions(
            audio_input=room_io.AudioInputOptions(
                noise_cancellation=noise_cancellation.BVC(),
            ),
            # The board renders no text, so skip streaming transcriptions to it.
            text_output=False,
        ),
    )

    await ctx.connect()


if __name__ == "__main__":
    cli.run_app(server)
