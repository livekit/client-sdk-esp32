from dataclasses import dataclass
from typing import Annotated

from dotenv import load_dotenv
from livekit import agents
from livekit.agents import (
    FunctionTool,
    RoomInputOptions,
    ToolError,
    function_tool,
    get_job_context,
)
from livekit.agents.voice import Agent, AgentSession, RunContext
from livekit.plugins import google, noise_cancellation, silero
from pydantic import Field

from database import FakeDB, Product, find_product_by_id, product_instructions

load_dotenv(".env.local")


@dataclass
class Userdata:
    products: list[Product]


BASE_INSTRUCTIONS = """
    You are a helpful grocery assistant in charge of performing price checks and answering product questions for customers.
    You have a fun, witty personality.
    Assume a customer just approached you for help.

    ## Product Identification
    - In order to answer questions about a product, you will need to visually identify which product the customer is holding.
    - Based on the defined products, choose the one whose name and/or description best matches what you observe.
    - If you are unable to get a good enough view of the product to identify it, ask the customer to try holding it closer or at a different angle.
    - If the item is recognizable but not in the defined product list, inform the customer that it isn't in inventory.

    ## Providing Help

    - Once you have identified the product, report its unit price.
    - If the product is on sale, mention it.
    - If the product is sold by weight, ask the customer to place it on the scale so you can check the actual price.

    ## Example Interaction

    - Customer: *holding a lime* "How much is this?"
    - Assistant: "Limes are $2.50 per kg. Would you like me to check the price using the scale?"
    - Customer: *places lime on scale* "Yes, please."
    - Assistant: *performs the price check* "I'm reading 0.125kg, so that'll be $0.31, an amazing deal!"
"""


class Assistant(Agent):
    def __init__(self, *, userdata: Userdata) -> None:
        instructions = (
            BASE_INSTRUCTIONS + "\n\n" + product_instructions(userdata.products)
        )
        super().__init__(
            instructions=instructions,
            tools=[self.build_check_price_via_scale(products=userdata.products)],
        )

    def build_check_price_via_scale(self, *, products: list[Product]) -> FunctionTool:
        product_ids = {item.id for item in products if item.scheme == "per_kg"}

        @function_tool
        async def check_price_via_scale(
            ctx: RunContext[Userdata],
            product_id: Annotated[
                str,
                Field(
                    description="The ID of the product being weighed (must be sold by weight).",
                    json_schema_extra={"enum": list(product_ids)},
                ),
            ],
        ):
            """
            Call this to read the weight of the item(s) placed on the scale and
            calculate the price of the product accordingly.
            """

            product = find_product_by_id(ctx.userdata.products, product_id)
            if not product:
                raise ToolError(f"Product '{product_id}' not found")

            try:
                reading = await read_scale_kg()
            except Exception as e:
                raise ToolError("Failed to read scale") from e

            price = reading * product.price
            return f"Scale reading: {reading:.2f}kg, price: ${price:.2f}."

        return check_price_via_scale


# If enabled, RPC calls will not be performed and a constant
# reading of 0.125kg will be returned.
TEST_MODE = False

async def read_scale_kg() -> float:
    if TEST_MODE:
        return 0.125

    room = get_job_context().room

    try:
        participant_identity = next(iter(room.remote_participants))
    except StopIteration as e:
        raise RuntimeError("No remote participants available") from e

    response = await room.local_participant.perform_rpc(
        destination_identity=participant_identity,
        method="read_scale_kg",
        response_timeout=15,
        payload="",
    )

    if isinstance(response, str):
        try:
            return float(response)
        except ValueError as e:
            raise RuntimeError("Received invalid weight value") from e

    raise RuntimeError("Unexpected response format")


async def new_userdata() -> Userdata:
    fake_db = FakeDB()
    products = await fake_db.list_products()
    return Userdata(products=products)


async def entrypoint(ctx: agents.JobContext):
    await ctx.connect()

    userdata = await new_userdata()
    session = AgentSession[Userdata](
        userdata=userdata,
        llm=google.beta.realtime.RealtimeModel(),
        vad=silero.VAD.load(),
    )
    await session.start(
        agent=Assistant(userdata=userdata),
        room=ctx.room,
        room_input_options=RoomInputOptions(
            video_enabled=True,
            noise_cancellation=noise_cancellation.BVC(),
        ),
    )


if __name__ == "__main__":
    agents.cli.run_app(agents.WorkerOptions(entrypoint_fnc=entrypoint))
