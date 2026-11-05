"""L'espace pour vos prochaines idées."""

from math import cos, pi, sin
from couleurs import PALETTE

def constellation(nombre: int = 6) -> list[tuple]:
    """Quelques étoiles, une infinité de possibles."""
    etoiles = []
    for i in range(nombre):
        angle = 2 * pi * i / nombre
        x = round(cos(angle) * 120, 1)
        y = round(sin(angle) * 120, 1)
        etoiles.append((x, y))
    return etoiles

if __name__ == "__main__":
    print("Votre prochaine idée commence ici.\n")
    for i, point in enumerate(constellation(), start=1):
        print(f"  étoile {i:02d}  →  {point}")
    print(f"\nPalette active : {PALETTE['menthe']}")
