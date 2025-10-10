import json
from enum import Enum
from dotenv import load_dotenv
from livekit import agents
from livekit.agents import (
    AgentSession,
    Agent,
    RunContext,
    RoomInputOptions,
    function_tool,
    get_job_context,
    ToolError,
)
from livekit.plugins import (
    openai,
    noise_cancellation
)

# If enabled, RPC calls will not be performed.
TEST_MODE = False

load_dotenv()

class LEDColor(str, Enum):
    RED = "red"
    BLUE = "blue"

class Assistant(Agent):
    def __init__(self) -> None:
        super().__init__(
            instructions="""You are a helpful voice AI assistant running on an ESP32 dev board.
            You answer user's questions about the hardware state and control the hardware based on their requests.
            The board has discrete LEDs that can be controlled independently. Each LED has a static color
            that cannot be changed. While you are able to set the state of the LEDs, you are not able to read the
            state which could be changed without your knowledge. No markdown is allowed in your responses.
            """
        )

    @function_tool()
    async def set_led_state(self, _: RunContext, led: LEDColor, state: bool) -> None:
        """Set the state of an on-board LED.

        Args:
            led: Which LED to set the state of.
            state: The state to set the LED to (i.e. on or off).
        """
        if TEST_MODE: return
        try:
            room = get_job_context().room
            participant_identity = next(iter(room.remote_participants))
            await room.local_participant.perform_rpc(
                destination_identity=participant_identity,
                method="set_led_state",
                payload=json.dumps({ "color": led.value, "state": state })
            )
        except Exception:
            raise ToolError("Unable to set LED state")

    @function_tool()
    async def get_cpu_temp(self, _: RunContext) -> float:
        """Get the current temperature of the CPU.

        Returns:
            The temperature reading in degrees Celsius.
        """
        if TEST_MODE: return 25.0
        try:
            room = get_job_context().room
            participant_identity = next(iter(room.remote_participants))
            response = await room.local_participant.perform_rpc(
                destination_identity=participant_identity,
                method="get_cpu_temp",
                response_timeout=10,
                payload=""
            )
            if isinstance(response, str):
                try:
                    response = float(response)
                except ValueError:
                    raise ToolError("Received invalid temperature value")
            return response
        except Exception:
            raise ToolError("Unable to retrieve CPU temperature")

async def entrypoint(ctx: agents.JobContext):
    session = AgentSession(
        llm=openai.realtime.RealtimeModel(
            voice="echo",
            model="gpt-4o-mini-realtime-preview-2024-12-17"
        )
    )
    await session.start(
        room=ctx.room,
        agent=Assistant(),
        room_input_options=RoomInputOptions(
            noise_cancellation=noise_cancellation.BVC()
        )
    )
    await ctx.connect()

if __name__ == "__main__":
    agents.cli.run_app(agents.WorkerOptions(entrypoint_fnc=entrypoint))