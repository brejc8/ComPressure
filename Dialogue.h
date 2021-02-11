#pragma once
#include <cstddef>
enum DialogueCharacter
{
    DIALOGUE_END,
    DIALOGUE_CHARLES,
    DIALOGUE_NICOLA,
    DIALOGUE_ADA,
    DIALOGUE_ANNIE,
};
struct Dialogue
{
    DialogueCharacter character;
    const char* text;
};

extern Dialogue* dialogue[];
