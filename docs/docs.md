https://docs.google.com/document/d/1XZQ5LlNYKmFDQ90AIR7qr0I_e7I-kYd2YgZsyE_Pomw/edit?usp=sharing (includes visual proof)
Chess AI Journal:
Planned length of the project: 6 Weeks
Source Code: https://github.com/hazlesh1/AI-ChessBOT 
Date: 2026/02/01
Developer: Leo Girard, (14)
Location: Netherlands Den Haag

Goal:
Setup The environment.
Create the basic Bitboard definition. (64-bit)

Implementation:
A 64-bit integer can hold the state of the board. This will allow parallel processing of moves using bitwise operators. 
 
Key Learning: 
64-Bit Architecture: Deepened my understanding of how a single 64-bit unsigned integer (uint64_t) can represent an entire 8x8 chessboard, allowing for highly efficient move calculations. 
Coordinate Mapping: Mastered the logic for converting 2D board coordinates (ranks and columns) into a 1D index (0-63). 
Bitwise Mastery: Gained practical experience using bitwise operators and the 1ULL constant to isolate and manipulate specific squares on the board.



Funny Facts during this day:
I was listening to the Frieren soundtrack the whole time with my new Denon PerL Pros. 
____________________________________________________________________________________
Date: 2026/02/08
Developer: Leo Girard, (14)
Location: Netherlands Den Haag

Goal: 
Transition from a single "test" bitboard to a complete Game State Manager. Implement an industry-standard FEN (Forsyth-Edwards Notation) parser to allow for dynamic position loading…

Implementation:
The State Suitcase (Structs): Instead of using loose variables, I encapsulated the entire game state into a Board struct. This includes an array of 12 bitboards (one for each piece type and color) and an integer to track the "side to move."

The FEN Parser (Data Ingestion): Built an interpreter that loops through a FEN string (e.g., rnbqkbnr/...). It translates ASCII characters into bitwise set_bit operations. This effectively creates a "Loading System" for the engine.

Composite Visualization: Upgraded the printer logic. Instead of printing 1s and 0s, the engine now scans all 12 bitboards simultaneously for every square and renders the correct piece character (P, N, R, etc.), providing a "Human-readable" view of the binary data.


Key Learning:
Encapsulation & Memory: Used memset to ensure the Board struct is zeroed out before use, preventing "garbage data" from previous memory states from corrupting the board.

Serialization: Learned how a standardized string (FEN) can represent a complex 2D state, and how to write a robust C-loop to parse it without crashing.

Abstraction with Enums: Moving away from "Magic Numbers." By using enum { P, N, B... }, the code is now self-documenting. I’m no longer telling the computer to "look at bitboard 1," I’m telling it to "look at the Knight bitboard."

Funny Facts during this day:
Watching Season 2 Frieren (Absolute peak). Wondering if I could implement my code with not just chess pieces, but with anime characters (yes, sounds very Otaku). 
____________________________________________________________________________________


About: 
The project:
This Project was developed using a “Human-in-the-loop” AI workflow.  
Developer:
I have a background in high-level Python development, specifically in creating complex physics simulations (gravity in liquids, collision physics). I chose to build this Chess Engine in C to challenge myself with low-level memory management and hardware-level optimization, concepts I am currently learning through Harvard’s CS50x. 





