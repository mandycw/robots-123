#include "Application.h"
#include "imgui/imgui.h"
#include <chrono>
#include "classes/AstroBots.h"

namespace ClassGame {
        //
        // our global variables
        //
        Game *game = nullptr;
        bool gameOver = false;
        int gameWinner = -1;

        // Frame rate limiter for AstroBots (30Hz = 33.33ms per frame)
        static auto lastAstroBotsUpdate = std::chrono::steady_clock::now();
        static constexpr double ASTROBOTS_UPDATE_INTERVAL_MS = 1000.0 / 30.0;  // 30 Hz

        //
        // game starting point
        // this is called by the main render loop in main.cpp
        //
        void GameStartUp() 
        {
            game = nullptr;
        }

        //
        // game render loop
        // this is called by the main render loop in main.cpp
        //
        void RenderGame() 
        {
                ImGui::DockSpaceOverViewport();

                //ImGui::ShowDemoWindow();

                ImGui::Begin("Settings");

                if (gameOver) {
                    ImGui::Text("Game Over!");
                    ImGui::Text("Winner: %d", gameWinner);
                    if (ImGui::Button("Reset Game")) {
                        game->stopGame();
                        game->setUpBoard();
                        gameOver = false;
                        gameWinner = -1;
                    }
                }
                if (!game) {

                    if (ImGui::Button("Start AstroBots")) {
                        game = new AstroBots();
                        game->setUpBoard();
                    }
                } else {
                    AstroBots *astroGame = dynamic_cast<AstroBots*>(game);
                    if (astroGame) {
                        auto now = std::chrono::steady_clock::now();
                        double elapsedMs = std::chrono::duration<double, std::milli>(now - lastAstroBotsUpdate).count();
                        if (elapsedMs >= ASTROBOTS_UPDATE_INTERVAL_MS) {
                            astroGame->endTurn();
                            lastAstroBotsUpdate = now;
                        }
                    } else {
                        ImGui::Text("Current Player Number: %d", game->getCurrentPlayer()->playerNumber());
                        std::string stateString = game->stateString();
                        int stride = game->_gameOptions.rowX;
                        int height = game->_gameOptions.rowY;

                        for(int y=0; y<height; y++) {
                            ImGui::Text("%s", stateString.substr(y*stride,stride).c_str());
                        }
                    }
                    ImGui::Text("Current Board State: %s", game->stateString().c_str());
                }
                ImGui::End();

                ImGui::Begin("GameWindow");
                if (game) {
                    if (game->gameHasAI() && (game->getCurrentPlayer()->isAIPlayer() || game->_gameOptions.AIvsAI))
                    {
                        game->updateAI();
                    }
                    game->drawFrame();
                }
                ImGui::End();
        }

        //
        // end turn is called by the game code at the end of each turn
        // this is where we check for a winner
        //
        void EndOfTurn() 
        {
            Player *winner = game->checkForWinner();
            if (winner)
            {
                gameOver = true;
                gameWinner = winner->playerNumber();
            }
            if (game->checkForDraw()) {
                gameOver = true;
                gameWinner = -1;
            }
        }
}
