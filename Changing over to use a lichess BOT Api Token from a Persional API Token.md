I have a webpage that is hosted on an esp32 micro-controller board. The html displays a full chessboard with all white and black chess pieces. A user typically will press a New Game button to start a new game session. The user uses the mouse or finger (if browser is on an ios screen) to make moves. After the user moves, the move is sent to lichess.org using a personal api. Lichess responds and the webpage moves the opponents piece to the correct place on the board. This works great, but lichess restrictions are such that only one game at a time is allowed for a Personal API token. This is a limiting factor that my website can only allow one user to play at a time. What I want is to be able to have multiple users on my website that are playing completely separate games, at the same time, with is impossible using a Personal Api token. I i was using a BOT token instead would multiple games be played using the same BOT token?
ChatGPT said:
Thought for 33s
Short answer: Yes. A single Lichess BOT account (one BOT token) can play many games at the same time. Lots of public bots do exactly this (their profiles even say so). 
lichess.org
A few key points so this works for your ESP32-hosted site:
Use a BOT account, not a personal account. You must �upgrade� a fresh Lichess account (one that has never played a game) to BOT; only BOT accounts can use the Bot API. 
lichess.org
Concurrency is on you. Your server/app needs to handle multiple game streams concurrently (one event stream per game: /api/stream/event + /api/bot/game/stream/{gameId}), and route each user�s moves to the correct game. (This is how existing bots juggle dozens of games.) Example bios explicitly note playing multiple games at once. 
lichess.org
Rate limits still apply. Lichess asks that you make one request at a time and, if you ever get HTTP 429, wait a full minute before resuming. Design your code with queued requests and persistent streams. 
lichess.org
Why your personal token felt �one at a time.� Regular player accounts are generally not meant to run multiple simultaneous real-time games (outside of simuls/correspondence), so trying to pipe many website users through your personal account hits that limitation. 
lichess.org
Practical architecture tweak
Have your webpage talk to a small backend (can be on the ESP32 if you prefer, but a tiny server helps) that holds the BOT token, creates/accepts challenges on behalf of the single bot account, and spins up a game-stream handler per user session. Each browser tab is then a separate game vs your bot, all in parallel.
If you ever outgrow one bot, Lichess allows a small number of accounts per person (don�t overdo it), but in practice one well-implemented bot is plenty. 
lichess.org
If you want, I can sketch a minimal flow (endpoints + event loop) that plugs into your current �New Game ? user moves ? Lichess replies� pipeline.
You said:
my esp32 and html/javascript code already has multiple session management built in and working. My session manager knows how to keep track of which session a response is meant for. So the one request at a time means you cant have multiple users simultaneously moving pieces at various times. The session manager would have to queue up all requests and process each request one at a time, which means any delaly the user experiences will be based on where their request is in the queue plus the ipchess system delays and network delays. Correct?
ChatGPT said:
Thought for 32s
Great news: your session manager can keep many games live at once, and you don�t have to block gameplay�as long as you respect Lichess�s request rule. Here�s the nuance:
Lichess�s rule is: �Only make one request at a time; if you get 429, wait a full minute.� 
lichess.org
That rule applies to HTTP requests you send (POST / move, create/accept challenge, resign, chat, etc.).
Game/event streams are NDJSON streaming connections you read; bots commonly keep several game streams open at once while they serialize their outbound requests. This is exactly how multi-game bots work in practice. 
lichess.org
+1
What this means for your site
Multiple users can move at the same time on your webpage; you read all their moves and the Lichess streams for each game concurrently. No need to pause the UI.
On the backend (ESP32 or a tiny relay server), put all Lichess outbound calls through a single queue (per token) so requests are sent one-by-one. Because each move is a tiny POST, the extra wait is usually a few milliseconds�users won�t notice unless you�re bursting many moves at once. The only big delay happens if you trip a 429 and must back off for a minute. 
lichess.org
Drop-in architecture pattern
One bot token (BOT account) handles all games.
Open streams concurrently:
/api/stream/event (global)
/api/bot/game/stream/{gameId} per active game
(This is standard for bot libraries.) 
Berserk Documentation
Outbound single-file queue: all POSTs (create/accept challenge, /move/{uci}, resign, chat) pass through a mutex/queue.
Fair scheduling: round-robin across games so a fast spammer can�t starve others.
429 handler: on 429, pause all sends for 60s, keep streams open, buffer moves, and resume. 
lichess.org
Bottom line
No, you don�t need to freeze users until the queue is empty. They can play simultaneously; you just serialize the actual HTTP requests to Lichess.
Yes, a single BOT token supports many parallel games; this is how public bots operate. Just follow the request rule and back-off guidance. 
lichess.org
If you want, I can sketch a tiny queue wrapper (JS or C++) you can drop into your current ESP32 flow.
You said:
i did not mean to freeze a user until the master session queue is empty, i meant a user would see a delay until that users request has sent the request, waited for the response back, hence a delay. Each user will see varying differences in queue delays depending on where in the queue their request is in the queue. A user should be blocked from sending another request until that users previous request has been fufilled or timed out.
ChatGPT said:
Thought for 7s
You�ve got it right.
Each browser/game can keep playing independently, but every outbound call to Lichess (for a given BOT token) must pass through a single-file request pipe.
So a player�s move experiences:
their own game�s �in-flight� lock (you already enforce �one request per user at a time�), plus
a small global queue delay while earlier requests (from other users) are sent, and
network + Lichess processing time for their request.
Different users will see slightly different latencies depending on where their request sits in the global queue.
And yes, each user should be blocked from sending another request until their previous request resolves or times out (that�s good UX and prevents turn/order bugs).
Here�s a clean pattern that works well in practice:
Two layers of flow control
Per-game lock (session-level �only one request at a time�)
Token-wide serializer (global �only one request in flight to Lichess�)
Minimal drop-in queue (browser/Node JS)
// ---- Token-wide serializer ----
class TokenRequestQueue {
  constructor() {
    this.q = [];
    this.running = false;
  }
  enqueue(taskFn) {
    return new Promise((resolve, reject) => {
      this.q.push({ taskFn, resolve, reject });
      if (!this.running) this._drain();
    });
  }
  async _drain() {
    this.running = true;
    while (this.q.length) {
      const { taskFn, resolve, reject } = this.q.shift();
      try {
        const result = await taskFn();    // <== exactly 1 request at a time
        resolve(result);
      } catch (err) {
        reject(err);
      }
    }
    this.running = false;
  }
}
const tokenQueue = new TokenRequestQueue();
// ---- Per-game lock ----
class GameSessionGate {
  constructor() { this.locked = false; }
  async runExclusive(fn) {
    while (this.locked) await new Promise(r => setTimeout(r, 5));
    this.locked = true;
    try { return await fn(); }
    finally { this.locked = false; }
  }
}
const sessions = new Map(); // gameId -> GameSessionGate
function gateFor(gameId) {
  if (!sessions.has(gameId)) sessions.set(gameId, new GameSessionGate());
  return sessions.get(gameId);
}
// ---- Lichess request wrapper with timeout/retry/429 backoff ----
async function lichessRequest(fetchArgs, { timeoutMs = 8000, retries = 2 } = {}) {
  let attempt = 0;
  while (true) {
    attempt++;
    const ctl = new AbortController();
    const t = setTimeout(() => ctl.abort('timeout'), timeoutMs);
    try {
      const res = await fetch(fetchArgs.url, { ...fetchArgs, signal: ctl.signal });
      clearTimeout(t);
      if (res.status === 429) {
        // Global backoff: pause the queue by delaying the *next* task
        await new Promise(r => setTimeout(r, 60_000));
        if (attempt <= retries) continue;
        throw new Error('429 Too Many Requests');
      }
      if (!res.ok) throw new Error(`HTTP ${res.status}`);
      return await res.json().catch(() => null); // or res.text() if needed
    } catch (err) {
      clearTimeout(t);
      if (attempt <= retries) {
        await new Promise(r => setTimeout(r, 300 * attempt));
        continue;
      }
      throw err;
    }
  }
}
// ---- Sending a move (example) ----
async function sendMove({ gameId, uci, token }) {
  return gateFor(gameId).runExclusive(() =>
    tokenQueue.enqueue(() =>
      lichessRequest({
        url: `https://lichess.org/api/bot/game/${gameId}/move/${uci}`,
        method: 'POST',
        headers: { 'Authorization': `Bearer ${token}` }
      })
    )
  );
}
UI/UX tips
Disable input for that one player while their move is in flight (or show a small spinner on the last moved piece). Other players stay interactive.
Optimistic move is fine (animate your user�s move immediately), then confirm or revert if Lichess rejects it.
Timeouts (e.g., 6�10s) + a retry or a �Try again� prompt keep things snappy.
Fair scheduling: if you worry about one game spamming, swap the tokenQueue implementation for a tiny round-robin across gameIds.
Latency intuition
If the average Lichess POST completes in ~100�200 ms, and there are a few concurrent games, the extra queue delay per move is typically small (tens to low hundreds of ms). The only big hit is if you ever get 429, where you must back off for ~60 s; your wrapper above does that automatically.
If you want, I can adapt this to your ESP32/Arduino (C++ with a simple FIFO + mutex) or to whatever tiny relay server you�re using.
You said:
why would a 429 error ever occur if only one request/reply is sent at a time?
ChatGPT said:
Thought for 10s
Short version: even with perfect serialization, a 429 can still happen because Lichess enforces multiple rate-limit strategies, not just �no parallel requests.� 
lichess.org
+1
Why you might still see 429:
Back-to-back bursts: �One at a time� doesn�t guarantee spacing. If you fire several POSTs in rapid succession (e.g., accept challenge ? move ? chat ? abort) you can still trip per-route or sliding-window limits. 
lichess.org
Per-endpoint quotas: Some endpoints have their own ceilings (e.g., import games has an hourly quota). Similar hidden ceilings may exist elsewhere. 
lichess.org
Shared limits by IP/token: Another process (or a second instance you forgot about) using the same token or IP can push you over the edge. Forum threads mention 429s from this kind of contention. 
lichess.org
Automatic retries / flapping connections: If your code retries quickly on network errors, you can unintentionally create micro-bursts that trigger 429. General guidance: treat 429 as a hard �slow down.� 
Stack Overflow
Operational throttling: During heavy load Lichess may tighten limits temporarily; the docs explicitly say to wait a full minute after any 429 before sending again. 
lichess.org
+1
Practical mitigations for your setup:
Global serializer + pacing: Keep the single outbound queue, but add a minimum inter-request gap (e.g., 75�150 ms with jitter).
Per-route cool-downs: Apply small extra delays after �heavier� endpoints (challenge create/accept, resign).
Strict 429 handling: On any 429, pause all sends for 60s, keep streams open, and buffer moves; then resume. (This is what Lichess asks for.) 
lichess.org
+1
Single writer, single token: Ensure only one running instance uses that BOT token; don�t let multiple devices/servers send on the same token.
No polling: Use the streaming endpoints for events and game states instead of periodic GETs; polling is a common cause of accidental rate pressure. 
lichess.org
So your understanding is right: users may see small, varying delays from queue position + network + Lichess processing. With gentle pacing and proper 429 backoff, those delays stay small and predictable.