// ts.h
#pragma once

void openTsFile (wchar_t* filename);
void closeTsFile();

void parsePackets (BYTE* samples, size_t sampleLen);
void parseFile (wchar_t* filename);
void parseFileMapped (wchar_t* filename);

void printServices();
void printPrograms();
void printPids();

void renderPidInfo (ID2D1DeviceContext* d2dContext, D2D1_SIZE_U client,
                    IDWriteTextFormat* textFormat,
                    ID2D1SolidColorBrush* whiteBrush,
                    ID2D1SolidColorBrush* blueBrush,
                    ID2D1SolidColorBrush* blackBrush,
                    ID2D1SolidColorBrush* greyBrush);
