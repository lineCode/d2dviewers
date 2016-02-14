// bda.h
#pragma once

void renderBDAGraph (ID2D1DeviceContext* d2dContext, D2D1_SIZE_F client,
                     IDWriteTextFormat* textFormat,
                     ID2D1SolidColorBrush* whiteBrush,
                     ID2D1SolidColorBrush* blueBrush,
                     ID2D1SolidColorBrush* blackBrush,
                     ID2D1SolidColorBrush* greyBrush);

uint8_t* getBDAend();
uint8_t* createBDAGraph (int freq);
