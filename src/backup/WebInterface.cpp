#include "WebInterface.h"
#include "GameController.h"
#include "WebScraper.h"

WebInterface::WebInterface() {
  server = nullptr;
  gameController = nullptr;
  webScraper = nullptr;

  // Initialize login status
  for (int i = 0; i < 2; i++) {
    loginStatus[i].serviceName = (i == 0) ? "ChatGPT" : "Gemini";
    loginStatus[i].isLoggedIn = false;
    loginStatus[i].username = "";
    loginStatus[i].lastLoginAttempt = "";
    loginStatus[i].errorMessage = "";
  }
}

void WebInterface::begin(AsyncWebServer* webServer) {
  server = webServer;

  // Serve static files
  server->on("/", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "text/html", getIndexHTML());
  });

  server->on("/style.css", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "text/css", getStyleCSS());
  });

  server->on("/script.js", HTTP_GET, [this](AsyncWebServerRequest* request) {
    request->send(200, "application/javascript", getScriptJS());
  });

  // API endpoints
  server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetStatus(request);
  });

  server->on("/api/board", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetBoard(request);
  });

  server->on("/api/moves", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleGetMoves(request);
  });

  server->on("/api/start", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleStartGame(request);
  });

  server->on("/api/pause", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handlePauseGame(request);
  });

  server->on("/api/resume", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleResumeGame(request);
  });

  server->on("/api/reset", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleResetGame(request);
  });

  // Login endpoints
  server->on("/login", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleLoginPage(request);
  });

  server->on("/login", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleLoginSubmit(request);
  });

  server->on("/api/login-status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    handleLoginStatus(request);
  });

  server->on("/api/logout", HTTP_POST, [this](AsyncWebServerRequest* request) {
    handleLogout(request);
  });

  Serial.println("Web interface routes configured");
}

void WebInterface::update() {
  // Periodic updates if needed
}

void WebInterface::handleGetStatus(AsyncWebServerRequest* request) {
  if (!gameController) {
    request->send(500, "application/json", "{\"error\":\"Game controller not initialized\"}");
    return;
  }

  JsonDocument status = gameController->getGameStatus();
  String response;
  serializeJson(status, response);

  request->send(200, "application/json", response);
}

void WebInterface::handleGetBoard(AsyncWebServerRequest* request) {
  String boardHTML = generateChessBoard();
  request->send(200, "text/html", boardHTML);
}

void WebInterface::handleGetMoves(AsyncWebServerRequest* request) {
  String movesHTML = generateMoveHistory();
  request->send(200, "text/html", movesHTML);
}

void WebInterface::handleStartGame(AsyncWebServerRequest* request) {
  if (!gameController) {
    request->send(500, "application/json", "{\"error\":\"Game controller not initialized\"}");
    return;
  }

  gameController->startNewGame();
  request->send(200, "application/json", "{\"success\":true,\"message\":\"New game started\"}");
}

void WebInterface::handlePauseGame(AsyncWebServerRequest* request) {
  if (!gameController) {
    request->send(500, "application/json", "{\"error\":\"Game controller not initialized\"}");
    return;
  }

  gameController->pauseGame();
  request->send(200, "application/json", "{\"success\":true,\"message\":\"Game paused\"}");
}

void WebInterface::handleResumeGame(AsyncWebServerRequest* request) {
  if (!gameController) {
    request->send(500, "application/json", "{\"error\":\"Game controller not initialized\"}");
    return;
  }

  gameController->resumeGame();
  request->send(200, "application/json", "{\"success\":true,\"message\":\"Game resumed\"}");
}

void WebInterface::handleResetGame(AsyncWebServerRequest* request) {
  if (!gameController) {
    request->send(500, "application/json", "{\"error\":\"Game controller not initialized\"}");
    return;
  }

  gameController->startNewGame();
  request->send(200, "application/json", "{\"success\":true,\"message\":\"Game reset\"}");
}

String WebInterface::generateChessBoard() {
  String html = "<div class='chessboard'>";

  // This would generate the current board state
  // For now, return a placeholder
  for (int row = 0; row < 8; row++) {
    html += "<div class='board-row'>";
    for (int col = 0; col < 8; col++) {
      String squareClass = ((row + col) % 2 == 0) ? "light-square" : "dark-square";
      html += "<div class='square " + squareClass + "' data-row='" + String(row) + "' data-col='" + String(col) + "'>";

      // Add piece if present (placeholder logic)
      String piece = "";
      if (row == 0 || row == 1 || row == 6 || row == 7) {
        piece = "♛"; // Placeholder piece
      }

      html += piece;
      html += "</div>";
    }
    html += "</div>";
  }

  html += "</div>";
  return html;
}

String WebInterface::generateGameStatus() {
  if (!gameController) {
    return "<div class='status'>Game controller not initialized</div>";
  }

  String status = "<div class='game-status'>";
  status += "<h3>Game Status</h3>";
  status += "<p>Game ID: " + gameController->getGameId() + "</p>";
  status += "<p>Current Player: " + gameController->getCurrentPlayer() + "</p>";
  status += "<p>Move Count: " + String(gameController->getMoveCount()) + "</p>";

  AIPlayer ai1 = gameController->getAI1();
  AIPlayer ai2 = gameController->getAI2();

  status += "<div class='ai-info'>";
  status += "<div class='ai1'><h4>" + ai1.name + " (White)</h4>";
  status += "<p>Last Move: " + ai1.lastMove + "</p>";
  status += "<p>Think Time: " + String(ai1.thinkTime) + "ms</p></div>";

  status += "<div class='ai2'><h4>" + ai2.name + " (Black)</h4>";
  status += "<p>Last Move: " + ai2.lastMove + "</p>";
  status += "<p>Think Time: " + String(ai2.thinkTime) + "ms</p></div>";
  status += "</div>";

  status += "</div>";
  return status;
}

String WebInterface::generateMoveHistory() {
  if (!gameController) {
    return "<div class='moves'>No game controller</div>";
  }

  // This would generate move history from the game controller
  String moves = "<div class='move-history'>";
  moves += "<h3>Move History</h3>";
  moves += "<div class='moves-list'>";

  // Placeholder move history
  moves += "<div class='move'>1. e4 e5</div>";
  moves += "<div class='move'>2. Nf3 Nc6</div>";
  moves += "<div class='move'>3. Bb5 a6</div>";

  moves += "</div>";
  moves += "</div>";
  return moves;
}

String WebInterface::getIndexHTML() {
  return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Chess AI vs AI</title>
    <link rel="stylesheet" href="/style.css">
</head>
<body>
    <div class="container">
        <nav class="menu-bar">
            <div class="menu-item">
                <a href="#" class="menu-link">File</a>
                <div class="dropdown">
                    <a href="#" onclick="startNewGame()">New Game</a>
                    <a href="#" onclick="saveGame()">Save Game</a>
                    <a href="#" onclick="loadGame()">Load Game</a>
                </div>
            </div>
            <div class="menu-item">
                <a href="#" class="menu-link">Actions</a>
                <div class="dropdown">
                    <a href="#" onclick="loginGemini()">Login Gemini</a>
                    <a href="#" onclick="loginChatGPT()">Login ChatGPT</a>
                    <a href="#" onclick="checkLoginStatus()">Check Login Status</a>
                    <a href="/login">Login Manager</a>
                </div>
            </div>
            <div class="menu-item">
                <a href="#" class="menu-link">Game</a>
                <div class="dropdown">
                    <a href="#" onclick="pauseGame()">Pause</a>
                    <a href="#" onclick="resumeGame()">Resume</a>
                    <a href="#" onclick="resetGame()">Reset</a>
                </div>
            </div>
            <div class="menu-item">
                <a href="#" class="menu-link">Help</a>
                <div class="dropdown">
                    <a href="#" onclick="showHelp()">About</a>
                    <a href="#" onclick="showStats()">Statistics</a>
                </div>
            </div>
        </nav>
        <header>
            <h1>Chess AI vs AI</h1>
            <div class="controls">
                <button id="startBtn" onclick="startGame()">Start New Game</button>
                <button id="pauseBtn" onclick="pauseGame()">Pause</button>
                <button id="resumeBtn" onclick="resumeGame()">Resume</button>
                <button id="resetBtn" onclick="resetGame()">Reset</button>
            </div>
        </header>
        <main>
            <div class="game-layout">
                <div class="board-section">
                    <div id="chessboard"></div>
                </div>
                <div class="info-section">
                    <div id="game-status"></div>
                    <div id="move-history"></div>
                </div>
            </div>
        </main>
        <footer>
            <div class="system-info">
                <div id="connection-status">Connected</div>
                <div id="last-update">Last update: <span id="timestamp"></span></div>
            </div>
        </footer>
    </div>
    <script src="/script.js"></script>
</body>
</html>)HTML";
}

String WebInterface::getStyleCSS() {
  return R"(
* {
    margin: 0;
    padding: 0;
    box-sizing: border-box;
}

body {
    font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
    background-color: #1a1a1a;
    color: #ffffff;
    line-height: 1.6;
}

.container {
    max-width: 1200px;
    margin: 0 auto;
    padding: 20px;
}

/* Menu Bar Styling */
.menu-bar {
    background-color: #2a2a2a;
    border-radius: 8px;
    padding: 0;
    margin-bottom: 20px;
    display: flex;
    box-shadow: 0 2px 4px rgba(0,0,0,0.3);
}

.menu-item {
    position: relative;
    display: inline-block;
}

.menu-link {
    display: block;
    padding: 15px 20px;
    color: #ffffff;
    text-decoration: none;
    border-right: 1px solid #3a3a3a;
    transition: background-color 0.3s ease;
}

.menu-link:hover {
    background-color: #4CAF50;
    color: white;
}

.menu-item:last-child .menu-link {
    border-right: none;
}

.dropdown {
    display: none;
    position: absolute;
    top: 100%;
    left: 0;
    background-color: #3a3a3a;
    min-width: 180px;
    box-shadow: 0 4px 8px rgba(0,0,0,0.3);
    z-index: 1000;
    border-radius: 0 0 8px 8px;
}

.dropdown a {
    display: block;
    padding: 12px 20px;
    color: #ffffff;
    text-decoration: none;
    border-bottom: 1px solid #4a4a4a;
    transition: background-color 0.3s ease;
}

.dropdown a:hover {
    background-color: #4CAF50;
}

.dropdown a:last-child {
    border-bottom: none;
    border-radius: 0 0 8px 8px;
}

.menu-item:hover .dropdown {
    display: block;
}

/* Menu responsive design */
@media (max-width: 768px) {
    .menu-bar {
        flex-direction: column;
    }

    .menu-link {
        border-right: none;
        border-bottom: 1px solid #3a3a3a;
    }

    .menu-item:last-child .menu-link {
        border-bottom: none;
    }

    .dropdown {
        position: static;
        box-shadow: none;
        background-color: #2a2a2a;
        margin-left: 20px;
    }
}

header {
    text-align: center;
    margin-bottom: 30px;
}

header h1 {
    color: #4CAF50;
    margin-bottom: 20px;
    font-size: 2.5em;
}

.controls {
    display: flex;
    justify-content: center;
    gap: 10px;
    flex-wrap: wrap;
}

.controls button {
    background-color: #4CAF50;
    color: white;
    border: none;
    padding: 10px 20px;
    border-radius: 5px;
    cursor: pointer;
    font-size: 14px;
    transition: background-color 0.3s;
}

.controls button:hover {
    background-color: #45a049;
}

.controls button:disabled {
    background-color: #666;
    cursor: not-allowed;
}

.game-layout {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 30px;
    margin-bottom: 30px;
}

@media (max-width: 768px) {
    .game-layout {
        grid-template-columns: 1fr;
    }
}

.chessboard {
    display: grid;
    grid-template-rows: repeat(8, 1fr);
    width: 400px;
    height: 400px;
    border: 2px solid #4CAF50;
    margin: 0 auto;
}

.board-row {
    display: grid;
    grid-template-columns: repeat(8, 1fr);
}

.square {
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 24px;
    cursor: pointer;
    transition: background-color 0.2s;
}

.light-square {
    background-color: #f0d9b5;
    color: #333;
}

.dark-square {
    background-color: #b58863;
    color: #333;
}

.square:hover {
    opacity: 0.8;
}

.info-section {
    background-color: #2a2a2a;
    border-radius: 10px;
    padding: 20px;
}

.game-status {
    margin-bottom: 20px;
}

.game-status h3 {
    color: #4CAF50;
    margin-bottom: 10px;
}

.ai-info {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 20px;
    margin-top: 15px;
}

.ai1, .ai2 {
    background-color: #3a3a3a;
    padding: 15px;
    border-radius: 5px;
}

.ai1 h4, .ai2 h4 {
    color: #FFD700;
    margin-bottom: 8px;
}

.move-history {
    background-color: #3a3a3a;
    border-radius: 5px;
    padding: 15px;
    max-height: 300px;
    overflow-y: auto;
}

.move-history h3 {
    color: #4CAF50;
    margin-bottom: 10px;
}

.moves-list {
    font-family: 'Courier New', monospace;
}

.move {
    padding: 2px 0;
    border-bottom: 1px solid #555;
}

.move:last-child {
    border-bottom: none;
}

footer {
    text-align: center;
    margin-top: 30px;
    padding-top: 20px;
    border-top: 1px solid #444;
}

.system-info {
    display: flex;
    justify-content: center;
    gap: 30px;
    flex-wrap: wrap;
}

#connection-status {
    color: #4CAF50;
    font-weight: bold;
}

#connection-status.disconnected {
    color: #f44336;
}

.loading {
    opacity: 0.6;
    pointer-events: none;
}

.error {
    color: #f44336;
    background-color: #ffebee;
    padding: 10px;
    border-radius: 5px;
    margin: 10px 0;
}

.success {
    color: #4CAF50;
    background-color: #e8f5e8;
    padding: 10px;
    border-radius: 5px;
    margin: 10px 0;
}
)";
}

String WebInterface::getScriptJS() {
  return R"(
let gameState = {
    connected: false,
    gameId: '',
    currentPlayer: '',
    moveCount: 0,
    status: 'idle'
};

// Initialize the interface
document.addEventListener('DOMContentLoaded', function() {
    console.log('Chess AI vs AI interface loaded');
    updateInterface();
    startPeriodicUpdates();
});

// Periodic updates
function startPeriodicUpdates() {
    setInterval(updateInterface, 2000); // Update every 2 seconds
}

// Update the entire interface
async function updateInterface() {
    try {
        await updateGameStatus();
        await updateBoard();
        await updateMoveHistory();
        updateTimestamp();
    } catch (error) {
        console.error('Error updating interface:', error);
        setConnectionStatus(false);
    }
}

// Update game status
async function updateGameStatus() {
    try {
        const response = await fetch('/api/status');
        if (!response.ok) throw new Error('Failed to fetch status');

        const status = await response.json();
        gameState = status;
        setConnectionStatus(true);

        // Update status display
        const statusDiv = document.getElementById('game-status');
        if (statusDiv) {
            statusDiv.innerHTML = `
                <div class="game-status">
                    <h3>Game Status</h3>
                    <p>Game ID: ${status.gameId || 'N/A'}</p>
                    <p>Current Player: ${status.currentPlayer || 'N/A'}</p>
                    <p>Move Count: ${status.moveCount || 0}</p>
                    <p>Status: ${status.status || 'Unknown'}</p>

                    <div class="ai-info">
                        <div class="ai1">
                            <h4>${status.ai1?.name || 'AI 1'} (White)</h4>
                            <p>Last Move: ${status.ai1?.lastMove || 'None'}</p>
                            <p>Think Time: ${status.ai1?.thinkTime || 0}ms</p>
                        </div>
                        <div class="ai2">
                            <h4>${status.ai2?.name || 'AI 2'} (Black)</h4>
                            <p>Last Move: ${status.ai2?.lastMove || 'None'}</p>
                            <p>Think Time: ${status.ai2?.thinkTime || 0}ms</p>
                        </div>
                    </div>
                </div>
            `;
        }

    } catch (error) {
        console.error('Error updating game status:', error);
        setConnectionStatus(false);
    }
}

// Update chess board
async function updateBoard() {
    try {
        const response = await fetch('/api/board');
        if (!response.ok) throw new Error('Failed to fetch board');

        const boardHTML = await response.text();
        const boardDiv = document.getElementById('chessboard');
        if (boardDiv) {
            boardDiv.innerHTML = boardHTML;
        }

    } catch (error) {
        console.error('Error updating board:', error);
    }
}

// Update move history
async function updateMoveHistory() {
    try {
        const response = await fetch('/api/moves');
        if (!response.ok) throw new Error('Failed to fetch moves');

        const movesHTML = await response.text();
        const movesDiv = document.getElementById('move-history');
        if (movesDiv) {
            movesDiv.innerHTML = movesHTML;
        }

    } catch (error) {
        console.error('Error updating move history:', error);
    }
}

// Game control functions
async function startGame() {
    try {
        const response = await fetch('/api/start', { method: 'POST' });
        if (!response.ok) throw new Error('Failed to start game');

        const result = await response.json();
        showMessage(result.message, 'success');
        updateInterface();

    } catch (error) {
        console.error('Error starting game:', error);
        showMessage('Failed to start game', 'error');
    }
}

async function pauseGame() {
    try {
        const response = await fetch('/api/pause', { method: 'POST' });
        if (!response.ok) throw new Error('Failed to pause game');

        const result = await response.json();
        showMessage(result.message, 'success');
        updateInterface();

    } catch (error) {
        console.error('Error pausing game:', error);
        showMessage('Failed to pause game', 'error');
    }
}

async function resumeGame() {
    try {
        const response = await fetch('/api/resume', { method: 'POST' });
        if (!response.ok) throw new Error('Failed to resume game');

        const result = await response.json();
        showMessage(result.message, 'success');
        updateInterface();

    } catch (error) {
        console.error('Error resuming game:', error);
        showMessage('Failed to resume game', 'error');
    }
}

async function resetGame() {
    try {
        if (!confirm('Are you sure you want to reset the current game?')) {
            return;
        }

        const response = await fetch('/api/reset', { method: 'POST' });
        if (!response.ok) throw new Error('Failed to reset game');

        const result = await response.json();
        showMessage(result.message, 'success');
        updateInterface();

    } catch (error) {
        console.error('Error resetting game:', error);
        showMessage('Failed to reset game', 'error');
    }
}

// Utility functions
function setConnectionStatus(connected) {
    gameState.connected = connected;
    const statusElement = document.getElementById('connection-status');
    if (statusElement) {
        statusElement.textContent = connected ? 'Connected' : 'Disconnected';
        statusElement.className = connected ? '' : 'disconnected';
    }
}

function updateTimestamp() {
    const timestampElement = document.getElementById('timestamp');
    if (timestampElement) {
        timestampElement.textContent = new Date().toLocaleTimeString();
    }
}

function showMessage(message, type = 'info') {
    // Create or update message display
    let messageDiv = document.getElementById('message-display');
    if (!messageDiv) {
        messageDiv = document.createElement('div');
        messageDiv.id = 'message-display';
        document.querySelector('.container').appendChild(messageDiv);
    }

    messageDiv.className = type;
    messageDiv.textContent = message;
    messageDiv.style.display = 'block';

    // Auto-hide after 3 seconds
    setTimeout(() => {
        messageDiv.style.display = 'none';
    }, 3000);
}

// Menu Functions
function loginGemini() {
    showMessage('Redirecting to Gemini login...', 'info');
    // Open login page with Gemini section focused
    const loginWindow = window.open('/login', '_blank');
    if (loginWindow) {
        loginWindow.focus();
        setTimeout(() => {
            try {
                loginWindow.document.getElementById('gemini-email').focus();
            } catch (e) {
                // Cross-origin restriction - ignore
            }
        }, 1000);
    }
}

function loginChatGPT() {
    showMessage('Redirecting to ChatGPT login...', 'info');
    // Open login page with ChatGPT section focused
    const loginWindow = window.open('/login', '_blank');
    if (loginWindow) {
        loginWindow.focus();
        setTimeout(() => {
            try {
                loginWindow.document.getElementById('chatgpt-email').focus();
            } catch (e) {
                // Cross-origin restriction - ignore
            }
        }, 1000);
    }
}

function checkLoginStatus() {
    fetch('/api/login-status')
        .then(response => response.json())
        .then(data => {
            let statusMessage = 'Login Status:\n';
            statusMessage += `ChatGPT: ${data.ai1.isLoggedIn ? '✓ Logged in as ' + data.ai1.username : '✗ Not logged in'}\n`;
            statusMessage += `Gemini: ${data.ai2.isLoggedIn ? '✓ Logged in as ' + data.ai2.username : '✗ Not logged in'}`;

            showMessage(statusMessage, data.ai1.isLoggedIn && data.ai2.isLoggedIn ? 'success' : 'info');
        })
        .catch(error => {
            console.error('Error checking login status:', error);
            showMessage('Failed to check login status', 'error');
        });
}

function startNewGame() {
    startGame();
}

function saveGame() {
    showMessage('Save game functionality coming soon...', 'info');
}

function loadGame() {
    showMessage('Load game functionality coming soon...', 'info');
}

function showHelp() {
    const helpText = `Chess AI vs AI Help:

1. Use the Actions menu to login to AI services
2. Both ChatGPT and Gemini must be logged in before starting a game
3. Use Game menu to control the chess match
4. Watch the real-time game status and move history

For more help, visit the Login Manager.`;

    showMessage(helpText, 'info');
}

function showStats() {
    showMessage('Statistics functionality coming soon...', 'info');
}

// Error handling
window.addEventListener('error', function(event) {
    console.error('JavaScript error:', event.error);
    showMessage('An error occurred in the interface', 'error');
});
)";
}

// Login page HTML
String WebInterface::getLoginHTML() {
  return R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Chess AI Login</title>
    <style>
        body { font-family: Arial, sans-serif; background: #1a1a1a; color: white; padding: 20px; }
        .login-container { max-width: 600px; margin: 0 auto; background: #2a2a2a; padding: 30px; border-radius: 10px; }
        .ai-section { margin: 20px 0; padding: 20px; background: #3a3a3a; border-radius: 8px; }
        .form-group { margin: 15px 0; }
        label { display: block; margin-bottom: 5px; font-weight: bold; }
        input { width: 100%; padding: 10px; border: 1px solid #555; background: #222; color: white; border-radius: 4px; }
        button { background: #4CAF50; color: white; padding: 12px 24px; border: none; border-radius: 4px; cursor: pointer; margin: 5px; }
        button:hover { background: #45a049; }
        .status { padding: 10px; margin: 10px 0; border-radius: 4px; }
        .success { background: #4CAF50; }
        .error { background: #f44336; }
        .pending { background: #ff9800; }
        .info { background: #2196F3; }
        h1, h2 { color: #4CAF50; }
        a { color: #4CAF50; text-decoration: none; }
        a:hover { text-decoration: underline; }
    </style>
</head>
<body>
    <div class="login-container">
        <h1>Chess AI Login Manager</h1>
        <p><a href="/">← Back to Chess Game</a></p>

        <div class="ai-section">
            <h2>ChatGPT (AI #1)</h2>
            <div id="chatgpt-status" class="status info">Not logged in</div>
            <form id="chatgpt-form">
                <div class="form-group">
                    <label for="chatgpt-email">Email:</label>
                    <input type="email" id="chatgpt-email" name="email" required>
                </div>
                <div class="form-group">
                    <label for="chatgpt-password">Password:</label>
                    <input type="password" id="chatgpt-password" name="password" required>
                </div>
                <button type="submit">Login to ChatGPT</button>
                <button type="button" onclick="logout(0)">Logout</button>
            </form>
        </div>

        <div class="ai-section">
            <h2>Gemini (AI #2)</h2>
            <div id="gemini-status" class="status info">Not logged in</div>
            <form id="gemini-form">
                <div class="form-group">
                    <label for="gemini-email">Email:</label>
                    <input type="email" id="gemini-email" name="email" required>
                </div>
                <div class="form-group">
                    <label for="gemini-password">Password:</label>
                    <input type="password" id="gemini-password" name="password" required>
                </div>
                <button type="submit">Login to Gemini</button>
                <button type="button" onclick="logout(1)">Logout</button>
            </form>
        </div>

        <div style="margin-top: 30px; text-align: center;">
            <button onclick="checkLoginStatus()">Refresh Status</button>
            <button onclick="window.location.href='/'">Start Chess Game</button>
        </div>
    </div>

    <script>
        // Check login status on page load
        checkLoginStatus();

        // Handle ChatGPT login
        document.getElementById('chatgpt-form').addEventListener('submit', function(e) {
            e.preventDefault();
            const email = document.getElementById('chatgpt-email').value;
            const password = document.getElementById('chatgpt-password').value;
            login(0, 'ChatGPT', email, password);
        });

        // Handle Gemini login
        document.getElementById('gemini-form').addEventListener('submit', function(e) {
            e.preventDefault();
            const email = document.getElementById('gemini-email').value;
            const password = document.getElementById('gemini-password').value;
            login(1, 'Gemini', email, password);
        });

        function login(aiIndex, serviceName, email, password) {
            const statusDiv = document.getElementById(aiIndex === 0 ? 'chatgpt-status' : 'gemini-status');
            statusDiv.className = 'status pending';
            statusDiv.textContent = `Logging in to ${serviceName}...`;

            const formData = new FormData();
            formData.append('ai_index', aiIndex);
            formData.append('service', serviceName);
            formData.append('email', email);
            formData.append('password', password);

            fetch('/login', {
                method: 'POST',
                body: formData
            })
            .then(response => response.text())
            .then(data => {
                if (data.includes('success')) {
                    statusDiv.className = 'status success';
                    statusDiv.textContent = `✓ Logged in to ${serviceName} as ${email}`;
                    // Clear password field
                    document.getElementById(aiIndex === 0 ? 'chatgpt-password' : 'gemini-password').value = '';
                } else {
                    statusDiv.className = 'status error';
                    statusDiv.textContent = `✗ Login failed: ${data}`;
                }
            })
            .catch(error => {
                statusDiv.className = 'status error';
                statusDiv.textContent = `✗ Login error: ${error.message}`;
            });
        }

        function logout(aiIndex) {
            const formData = new FormData();
            formData.append('ai_index', aiIndex);

            fetch('/api/logout', {
                method: 'POST',
                body: formData
            })
            .then(() => {
                checkLoginStatus();
            });
        }

        function checkLoginStatus() {
            fetch('/api/login-status')
            .then(response => response.json())
            .then(data => {
                updateStatus(0, data.ai1);
                updateStatus(1, data.ai2);
            })
            .catch(error => {
                console.error('Error checking login status:', error);
            });
        }

        function updateStatus(aiIndex, status) {
            const statusDiv = document.getElementById(aiIndex === 0 ? 'chatgpt-status' : 'gemini-status');

            if (status.isLoggedIn) {
                statusDiv.className = 'status success';
                statusDiv.textContent = `✓ Logged in as ${status.username}`;
            } else {
                statusDiv.className = 'status info';
                statusDiv.textContent = status.errorMessage || 'Not logged in';
            }
        }

        // Auto-refresh status every 30 seconds
        setInterval(checkLoginStatus, 30000);
    </script>
</body>
</html>)HTML";
}

// Login handlers
void WebInterface::handleLoginPage(AsyncWebServerRequest* request) {
  request->send(200, "text/html", getLoginHTML());
}

void WebInterface::handleLoginSubmit(AsyncWebServerRequest* request) {
  if (!webScraper) {
    request->send(500, "text/plain", "WebScraper not initialized");
    return;
  }

  String aiIndexStr = "";
  String service = "";
  String email = "";
  String password = "";

  // Extract form data
  if (request->hasParam("ai_index", true)) {
    aiIndexStr = request->getParam("ai_index", true)->value();
  }
  if (request->hasParam("service", true)) {
    service = request->getParam("service", true)->value();
  }
  if (request->hasParam("email", true)) {
    email = request->getParam("email", true)->value();
  }
  if (request->hasParam("password", true)) {
    password = request->getParam("password", true)->value();
  }

  int aiIndex = aiIndexStr.toInt();

  if (aiIndex < 0 || aiIndex >= 2 || email.isEmpty() || password.isEmpty()) {
    request->send(400, "text/plain", "Invalid parameters");
    return;
  }

  Serial.printf("Login attempt - AI: %d, Service: %s, Email: %s\n", aiIndex, service.c_str(), email.c_str());

  // Setup the AI endpoint
  if (service == "ChatGPT") {
    webScraper->setupChatGPT(aiIndex);
  } else if (service == "Gemini") {
    webScraper->setupGemini(aiIndex);
  } else {
    request->send(400, "text/plain", "Unknown service");
    return;
  }

  // Attempt login (this would need to be implemented in WebScraper)
  bool loginSuccess = webScraper->loginToService(aiIndex, email, password);

  if (loginSuccess) {
    updateLoginStatus(aiIndex, true, email);
    request->send(200, "text/plain", "success");
  } else {
    updateLoginStatus(aiIndex, false, "", "Login failed - check credentials");
    request->send(400, "text/plain", "Login failed - check credentials");
  }
}

void WebInterface::handleLoginStatus(AsyncWebServerRequest* request) {
  JsonDocument doc;

  JsonObject ai1 = doc["ai1"].to<JsonObject>();
  ai1["serviceName"] = loginStatus[0].serviceName;
  ai1["isLoggedIn"] = loginStatus[0].isLoggedIn;
  ai1["username"] = loginStatus[0].username;
  ai1["errorMessage"] = loginStatus[0].errorMessage;

  JsonObject ai2 = doc["ai2"].to<JsonObject>();
  ai2["serviceName"] = loginStatus[1].serviceName;
  ai2["isLoggedIn"] = loginStatus[1].isLoggedIn;
  ai2["username"] = loginStatus[1].username;
  ai2["errorMessage"] = loginStatus[1].errorMessage;

  String response;
  serializeJson(doc, response);
  request->send(200, "application/json", response);
}

void WebInterface::handleLogout(AsyncWebServerRequest* request) {
  if (!webScraper) {
    request->send(500, "text/plain", "WebScraper not initialized");
    return;
  }

  String aiIndexStr = "";
  if (request->hasParam("ai_index", true)) {
    aiIndexStr = request->getParam("ai_index", true)->value();
  }

  int aiIndex = aiIndexStr.toInt();
  if (aiIndex >= 0 && aiIndex < 2) {
    webScraper->clearSession(aiIndex);
    updateLoginStatus(aiIndex, false);
    request->send(200, "text/plain", "Logged out");
  } else {
    request->send(400, "text/plain", "Invalid AI index");
  }
}

void WebInterface::updateLoginStatus(int aiIndex, bool success, const String& username, const String& error) {
  if (aiIndex >= 0 && aiIndex < 2) {
    loginStatus[aiIndex].isLoggedIn = success;
    loginStatus[aiIndex].username = username;
    loginStatus[aiIndex].errorMessage = error;
    loginStatus[aiIndex].lastLoginAttempt = String(millis());

    Serial.printf("Login status updated - AI %d: %s\n", aiIndex + 1, success ? "SUCCESS" : "FAILED");
  }
}

bool WebInterface::isAILoggedIn(int aiIndex) {
  if (aiIndex >= 0 && aiIndex < 2) {
    return loginStatus[aiIndex].isLoggedIn;
  }
  return false;
}