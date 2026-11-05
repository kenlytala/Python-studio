"""Une palette pour les idées qui brillent."""

PALETTE = {
    "menthe": "#A4F5C0",
    "lagon": "#89DCEB",
    "lavande": "#C4ABF3",
    "soleil": "#F5D49A",
    "nuit": "#101218",
}


def hex_vers_rgb(couleur: str) -> tuple[int, int, int]:
    """Transforme une couleur hexadécimale en triplet RGB."""
    valeur = couleur.lstrip("#")
    if len(valeur) != 6:
        raise ValueError("Une couleur doit contenir 6 chiffres.")
    return tuple(int(valeur[i:i + 2], 16) for i in (0, 2, 4))


if __name__ == "__main__":
    for nom, couleur in PALETTE.items():
        print(f"{nom:>8}   {couleur}   {hex_vers_rgb(couleur)}")
