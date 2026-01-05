#pragma once

struct tObject;

// Occlusion / background-mask overlay helpers.
void loadMask(int cameraIdx);
void createAITD1Mask();
void drawBgOverlay(tObject* actorPtr);
