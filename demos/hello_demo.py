"""Speak a sentence through the best backend available."""

import prism

context = prism.Context()
backend = context.create_best()
backend.speak("Hello from Prism.", interrupt=True)
input(f"Using {backend.name}. Press Enter to exit.")
