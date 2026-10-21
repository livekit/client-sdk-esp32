from typing import Literal

from pydantic import BaseModel

PriceScheme = Literal["each", "per_kg"]


class Product(BaseModel):
    id: str
    name: str
    price: float
    scheme: PriceScheme
    is_sale: bool

class FakeDB:
    async def list_products(self) -> list[Product]:
        return [
            Product(
                id="P001",
                name="Avocado",
                price=1.75,
                scheme="each",
                is_sale=False,
            ),
            Product(
                id="P002",
                name="Tomato",
                price=1.25,
                scheme="per_kg",
                is_sale=True,
            ),
            Product(
                id="P003",
                name="Lime",
                price=2.50,
                scheme="per_kg",
                is_sale=True,
            ),
        ]

def find_product_by_id(
    items: list[Product],
    product_id: str
) -> list[Product]:
    return next(item for item in items if item.id == product_id)

def product_instructions(products: list[Product]) -> str:
    menu_lines = []
    for product in products:
        scheme_indicator = "ea." if product.scheme == "each" else "kg"
        sale_indicator = "sale" if product.is_sale else "regular"
        line = f"- [id={product.id}] {product.name}: {sale_indicator} ${product.price:.2f}/{scheme_indicator}"
        menu_lines.append(line)
    return "## Product inventory:\n" + "\n".join(menu_lines)
