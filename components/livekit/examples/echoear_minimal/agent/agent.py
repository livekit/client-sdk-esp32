from enum import Enum
import random
from dotenv import load_dotenv
from livekit import agents
from livekit.agents import (
    AgentSession,
    Agent,
    RunContext,
    RoomInputOptions,
    function_tool,
    ToolError,
)
from livekit.plugins import (
    openai,
    cartesia,
    deepgram,
    silero,
)
from livekit.plugins.turn_detector.english import EnglishModel

# If enabled, RPC calls will not be performed.
TEST_MODE = False

load_dotenv()

class LEDColor(str, Enum):
    RED = "red"
    BLUE = "blue"

class Assistant(Agent):
    def __init__(self) -> None:
        super().__init__(
            instructions="""You are a helpful voice AI assistant.
            You are able to assist the user with their requests by providing concise answers.
            Do not ask additional questions after the user's request is answered.
            No markdown or emojis are allowed in your responses.
            """
        )
    async def on_enter(self) -> None:
        await self.session.say(
            "Hi there.  How can I help you today?",
            allow_interruptions=False
        )
    
    @function_tool()
    async def get_weather(self, _: RunContext, location: str) -> str:
        """Get the current weather in a given location.

        Args:
            location: The location to get the weather for.

        Returns:
            The current temperature in the given location.
        """
        # generate random temperature between 60 and 80 degrees Fahrenheit
        temp = random.randint(60, 80)
        return f"The current temperature in {location} is {temp}°F."
        
    @function_tool()
    async def get_calendar_events(self, _: RunContext, datetime: str) -> str:
        """ Get the calendar events for a given datetime.

        Args:
            datetime: The datetime to get the calendar events for.

        Returns:
            The calendar events for the given datetime.
        """
        # pick a random number between 1 and 10, if the number is less than 3, return "no event at that time"
        if random.randint(1, 10) < 3:
            return "No events at that time."
        else:
            # generate a random name 
            name = random.choice(["David Chen", "Jacob Gelman", "Ben Cherry", "Russ D'sa"])
            
            call_or_meeting = random.choice(["call", "meeting"])
            
            if call_or_meeting == "call":
                return f"Call with {name} at {datetime}."
            else:
                location = random.choice(["office", "Ritual Coffee", "Blue Bottle Coffee Shop", "downtown"])
                time_away = random.randint(10, 30)
                return f"Meeting with {name} at {datetime} at {location}.  It's {time_away} minutes away."
    
    @function_tool()
    async def send_email(self, _: RunContext, recipient: str, body: str) -> str:
        """ Send an email to a given recipient.

        Args:
            recipient: The recipient of the email.
            body: The body of the email.
        """
        return f"Email sent to {recipient}."
        
async def entrypoint(ctx: agents.JobContext):
    session = AgentSession(
        stt=deepgram.STT(model="nova-3", language="multi"),
        llm=openai.LLM(model="gpt-4o-mini"),
        tts=cartesia.TTS(model="sonic-2", voice="32b3f3c5-7171-46aa-abe7-b598964aa793"),
        vad=silero.VAD.load(),
        # turn_detection=EnglishModel(),
    )
    await session.start(
        room=ctx.room,
        agent=Assistant(),
        room_input_options=RoomInputOptions()
    )
    await ctx.connect()

if __name__ == "__main__":
    agents.cli.run_app(agents.WorkerOptions(entrypoint_fnc=entrypoint))