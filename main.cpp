#ifdef _WIN32
#include <windows.h>
#endif
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

const int W = 960, H = 720;
const float PI = 3.14159f, BR = 8.5f, PY = 50, BPW = 120, PH = 18, MAXSP = 760;

int winW = W, winH = H, lastMs = 0;

enum Screen
{
    MENU,
    PLAY,
    PAUSE,
    HELP,
    HIGHSCORE,
    GAMEOVER,
    LEVELDONE
};
enum DType
{
    D_LIFE,
    D_WIDE,
    D_FAST,
    D_MULTI,
    D_MAGNET,
    D_SHIELD,
    D_LASER,
    D_SLOW,
    D_SHRINK,
    D_DEATH
};

struct Col
{
    float r, g, b;
};
struct Paddle
{
    float x, y, w, h, baseW, sizeEnd, magEnd, lasEnd, lastLas;
    int shield;
};
struct Ball
{
    float x, y, px, py, r, vx, vy;
    bool stuck, active;
};
struct Brick
{
    float x, y, baseX, w, h;
    int hp, maxHp;
    bool alive, bomb, boss, moving;
    float phase, amp;
    Col color;
};
struct Drop
{
    float x, y, vy, size;
    DType type;
    bool active;
};
struct Bullet
{
    float x, y, w, h, vy;
    bool active;
};
struct Particle
{
    float x, y, vx, vy, life, maxLife;
    Col color;
};

Screen scr = MENU;
Paddle pad;
std::vector<Ball> balls;
std::vector<Brick> bricks;
std::vector<Drop> drops;
std::vector<Bullet> bullets;
std::vector<Particle> parts;

int score = 0, highScore = 0, lives = 3, lvl = 1, combo = 0;
float comboUntil = 0, gt = 0, slowUntil = 0, ldReal = 0, bossDrop = 0;
bool hasGame = false, kL = false, kR = false, soundOn = true;
std::string notice = "";
float noticeUntil = 0;

// ============ UTILITY ============
Col C(float r, float g, float b)
{
    Col c = {r, g, b};
    return c;
}
float rnd() { return (float)rand() / RAND_MAX; }
float clp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
float rt() { return glutGet(GLUT_ELAPSED_TIME) / 1000.0f; }

std::string ttext(float t)
{
    std::ostringstream o;
    o << (int)t / 60 << ":" << std::setw(2) << std::setfill('0') << (int)t % 60;
    return o.str();
}

void sfx(int f, int ms)
{
    if (!soundOn)
        return;
#ifdef _WIN32
    Beep(f, ms);
#endif
}

void note(const std::string &s, float sec = 2)
{
    notice = s;
    noticeUntil = gt + sec;
}

void saveHigh()
{
    if (score > highScore)
    {
        highScore = score;
        std::ofstream f("highscore.txt");
        f << highScore;
    }
}

void loadHigh()
{
    std::ifstream f("highscore.txt");
    if (f)
        f >> highScore;
}

void clampPad() { pad.x = clp(pad.x, 8, W - pad.w - 8); }
float bSpd(const Ball &b) { return std::sqrt(b.vx * b.vx + b.vy * b.vy); }

void setBSpd(Ball &b, float sp)
{
    float s = bSpd(b);
    if (s < 1)
    {
        b.vx = sp * 0.4f;
        b.vy = sp * 0.9f;
        return;
    }
    b.vx = b.vx / s * sp;
    b.vy = b.vy / s * sp;
}

bool rOver(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh)
{
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

bool cRect(float cx, float cy, float r, float rx, float ry, float rw, float rh)
{
    float nx = clp(cx, rx, rx + rw), ny = clp(cy, ry, ry + rh);
    float dx = cx - nx, dy = cy - ny;
    return dx * dx + dy * dy <= r * r;
}

// ============ DRAW HELPERS ============
void rect(float x, float y, float w, float h, Col c)
{
    glColor3f(c.r, c.g, c.b);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void rectA(float x, float y, float w, float h, Col c, float a)
{
    glColor4f(c.r, c.g, c.b, a);
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void outl(float x, float y, float w, float h, Col c)
{
    glColor3f(c.r, c.g, c.b);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void circ(float cx, float cy, float r, Col c, float a = 1, int seg = 32)
{
    glColor4f(c.r, c.g, c.b, a);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int i = 0; i <= seg; i++)
    {
        float ang = 2 * PI * i / seg;
        glVertex2f(cx + cos(ang) * r, cy + sin(ang) * r);
    }
    glEnd();
}

void ring(float cx, float cy, float r, Col c, float a = 1)
{
    glColor4f(c.r, c.g, c.b, a);
    glBegin(GL_LINE_LOOP);
    for (int i = 0; i < 48; i++)
    {
        float ang = 2 * PI * i / 48;
        glVertex2f(cx + cos(ang) * r, cy + sin(ang) * r);
    }
    glEnd();
}

int tw(void *f, const std::string &s)
{
    int w = 0;
    for (char c : s)
        w += glutBitmapWidth(f, c);
    return w;
}

void text(float x, float y, const std::string &s, Col c = C(1, 1, 1), void *f = GLUT_BITMAP_HELVETICA_18)
{
    glColor3f(c.r, c.g, c.b);
    glRasterPos2f(x, y);
    for (char ch : s)
        glutBitmapCharacter(f, ch);
}

void ctext(float cx, float y, const std::string &s, Col c = C(1, 1, 1), void *f = GLUT_BITMAP_HELVETICA_18)
{
    text(cx - tw(f, s) / 2.0f, y, s, c, f);
}

void btn(float x, float y, float w, float h, const std::string &l, bool a = true)
{
    rect(x, y, w, h, a ? C(0.06f, 0.14f, 0.30f) : C(0.12f, 0.12f, 0.12f));
    outl(x, y, w, h, a ? C(0.2f, 0.95f, 1) : C(0.4f, 0.4f, 0.4f));
    ctext(x + w / 2, y + h / 2 - 6, l, a ? C(1, 1, 1) : C(0.55f, 0.55f, 0.55f));
}

bool ins(float px, float py, float x, float y, float w, float h)
{
    return px >= x && px <= x + w && py >= y && py <= y + h;
}

// ============ GAME SETUP ============
void resetRound()
{
    drops.clear();
    bullets.clear();
    pad.baseW = BPW;
    pad.w = BPW;
    pad.h = PH;
    pad.x = W / 2.0f - pad.w / 2;
    pad.y = PY;
    pad.sizeEnd = pad.magEnd = pad.lasEnd = 0;
    pad.lastLas = -10;
    balls.clear();
    Ball b = {pad.x + pad.w / 2, pad.y + pad.h + BR + 1, 0, 0, BR,
              rnd() < 0.5f ? -150.0f : 150.0f, 340.0f + lvl * 10, true, true};
    b.px = b.x;
    b.py = b.y;
    balls.push_back(b);
}

void makeLevel()
{
    bricks.clear();
    drops.clear();
    bullets.clear();
    parts.clear();
    bool bossLv = (lvl % 3 == 0);
    int cols = 10 + std::min(lvl / 2, 4);
    int rows = bossLv ? 5 : std::min(6 + lvl, 10);
    float gap = 5, mg = 40;
    float bw = (W - 2 * mg - (cols - 1) * gap) / cols, bh = 24;
    float sy = bossLv ? H - 240.0f : H - 120.0f;

    Col pal[6] = {
        C(1, 0.15f, 0.20f), C(1, 0.55f, 0.10f), C(0.95f, 0.90f, 0.15f),
        C(0.10f, 0.95f, 0.25f), C(0.10f, 0.70f, 1), C(0.85f, 0.20f, 1)};

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            Brick b;
            b.x = mg + c * (bw + gap);
            b.baseX = b.x;
            b.y = sy - r * (bh + gap);
            b.w = bw;
            b.h = bh;
            b.alive = true;
            b.bomb = false;
            b.boss = false;
            b.moving = false;
            b.phase = rnd() * 6;
            b.amp = 0;
            b.hp = 1;
            b.maxHp = 1;
            b.color = pal[(r + c + lvl) % 6];
            if ((r + c + lvl) % 7 == 0)
            {
                b.hp = b.maxHp = 2;
                b.color = C(0.75f, 0.20f, 0.95f);
            }
            if ((r * cols + c + lvl) % 13 == 0)
            {
                b.bomb = true;
                b.color = C(1, 0.25f, 0.05f);
            }
            if (lvl >= 2 && r % 2 == 0 && c % 3 == 0)
            {
                b.moving = true;
                b.amp = 22 + lvl * 3;
            }
            bricks.push_back(b);
        }
    }

    if (bossLv)
    {
        Brick bs;
        bs.w = 280;
        bs.h = 46;
        bs.x = W / 2.0f - bs.w / 2;
        bs.baseX = bs.x;
        bs.y = H - 145;
        bs.hp = bs.maxHp = 16 + lvl * 4;
        bs.alive = true;
        bs.bomb = false;
        bs.boss = true;
        bs.moving = true;
        bs.phase = 0;
        bs.amp = 210;
        bs.color = C(1, 0.05f, 0.45f);
        bricks.push_back(bs);
        bossDrop = gt + 4;
    }
}

void startNew()
{
    score = 0;
    lives = 3;
    lvl = 1;
    combo = 0;
    gt = 0;
    slowUntil = 0;
    pad.shield = 0;
    makeLevel();
    resetRound();
    hasGame = true;
    scr = PLAY;
    note("Game Started!", 1.5f);
}

void nextLvl()
{
    lvl++;
    combo = 0;
    slowUntil = 0;
    pad.shield = 0;
    makeLevel();
    resetRound();
    scr = PLAY;
    hasGame = true;
    note("Level " + std::to_string(lvl), 1.5f);
    sfx(1300, 70);
}

void resumeG()
{
    if (hasGame)
        scr = PLAY;
}

bool allGone()
{
    for (auto &b : bricks)
        if (b.alive)
            return false;
    return true;
}

void levelDone()
{
    saveHigh();
    scr = LEVELDONE;
    ldReal = rt();
    note("Level Cleared!", 2);
    sfx(1500, 100);
}

void loseLife()
{
    lives--;
    combo = 0;
    sfx(220, 120);
    if (lives <= 0)
    {
        saveHigh();
        hasGame = false;
        scr = GAMEOVER;
        note("Game Over", 2);
    }
    else
    {
        resetRound();
        note("Life Lost!", 1.5f);
    }
}

// ============ DROPS ============
std::string dLbl(DType t)
{
    const char *lbls[] = {"L", "W", "F", "M", "G", "D", "Z", "T", "S", "X"};
    return lbls[t];
}

std::string dName(DType t)
{
    const char *names[] = {"Extra Life", "Wide Paddle", "Faster Ball", "Multiball",
                           "Magnet Paddle", "Shield Barrier", "Laser Paddle", "Time Rift Slow",
                           "Shrink Paddle", "Death Drop"};
    return names[t];
}

Col dCol(DType t)
{
    Col cs[] = {C(0.1f, 1, 0.2f), C(0.2f, 0.8f, 1), C(1, 0.55f, 0), C(0.95f, 0.95f, 0.15f),
                C(0, 1, 1), C(0.35f, 0.45f, 1), C(1, 0.15f, 0.15f), C(0.65f, 0.35f, 1),
                C(1, 1, 1), C(1, 0, 0.75f)};
    return cs[t];
}

bool badD(DType t) { return t == D_DEATH || t == D_SHRINK; }

void mkDrop(float x, float y, DType t)
{
    Drop d = {x, y, -125, 24, t, true};
    drops.push_back(d);
}

void rndDrop(float x, float y)
{
    float ch = 0.28f + std::min(0.15f, lvl * 0.015f);
    if (rnd() > ch)
        return;
    float r = rnd();
    DType t = r < 0.11f ? D_LIFE : r < 0.24f ? D_WIDE
                               : r < 0.36f   ? D_FAST
                               : r < 0.49f   ? D_MULTI
                               : r < 0.61f   ? D_MAGNET
                               : r < 0.73f   ? D_SHIELD
                               : r < 0.84f   ? D_LASER
                               : r < 0.93f   ? D_SLOW
                               : r < 0.975f  ? D_SHRINK
                                             : D_DEATH;
    mkDrop(x, y, t);
}

void burst(float x, float y, Col c, int n = 22)
{
    for (int i = 0; i < n; i++)
    {
        float ang = rnd() * 2 * PI, sp = 50 + rnd() * 220;
        Particle p = {x, y, cos(ang) * sp, sin(ang) * sp, 0.35f + rnd() * 0.45f, 0, c};
        p.maxLife = p.life;
        parts.push_back(p);
    }
}

void explodeAt(float x, float y);

void dmgBrick(size_t id, int dmg, bool byExp = false)
{
    if (id >= bricks.size())
        return;
    Brick &b = bricks[id];
    if (!b.alive)
        return;
    b.hp -= dmg;
    float cx = b.x + b.w / 2, cy = b.y + b.h / 2;
    burst(cx, cy, b.color, b.boss ? 10 : 5);

    if (b.hp <= 0)
    {
        bool wb = b.bomb, ws = b.boss;
        Col bc = b.color;
        b.alive = false;
        combo++;
        comboUntil = gt + 2.6f;
        int mult = std::min(5, 1 + combo / 4);
        int gain = (45 + b.maxHp * 20 + lvl * 10) * mult;
        if (ws)
            gain += 600;
        score += gain;
        rndDrop(cx, cy);
        burst(cx, cy, bc, ws ? 80 : 24);
        if (combo >= 4)
            note("Combo x" + std::to_string(mult), 1.2f);
        if (wb && !byExp)
        {
            note("Bomb Chain!", 1.2f);
            explodeAt(cx, cy);
        }
        if (ws)
            note("Boss Destroyed!", 2);
        sfx(ws ? 1500 : 950, ws ? 90 : 25);
    }
    else
    {
        score += 4;
        sfx(540, 15);
    }
}

void explodeAt(float x, float y)
{
    for (size_t i = 0; i < bricks.size(); i++)
    {
        if (!bricks[i].alive)
            continue;
        float cx = bricks[i].x + bricks[i].w / 2, cy = bricks[i].y + bricks[i].h / 2;
        float dx = cx - x, dy = cy - y;
        if (dx * dx + dy * dy < 105 * 105)
            dmgBrick(i, 2, true);
    }
    burst(x, y, C(1, 0.45f, 0.05f), 70);
    sfx(250, 60);
}

void setPadW(float nw, float until)
{
    float ctr = pad.x + pad.w / 2;
    pad.w = nw;
    pad.x = ctr - pad.w / 2;
    pad.sizeEnd = until;
    clampPad();
}

void addMulti()
{
    std::vector<Ball> base = balls;
    for (auto &b : balls)
        b.stuck = false;
    for (size_t i = 0; i < base.size() && balls.size() < 8; i++)
    {
        if (!base[i].active)
            continue;
        float sp = std::max(340.0f, bSpd(base[i]));
        float ang = std::atan2(std::fabs(base[i].vy), base[i].vx);
        for (int k = 0; k < 2 && balls.size() < 8; k++)
        {
            Ball nb = base[i];
            float off = (k == 0 ? -0.55f : 0.55f);
            nb.stuck = false;
            nb.active = true;
            nb.vx = cos(ang + off) * sp;
            nb.vy = std::fabs(sin(ang + off) * sp);
            if (nb.vy < 120)
                nb.vy = 220;
            balls.push_back(nb);
        }
    }
}

void applyDrop(DType t)
{
    note(dName(t), 1.4f);
    switch (t)
    {
    case D_LIFE:
        lives++;
        sfx(1200, 60);
        break;
    case D_WIDE:
        setPadW(175, gt + 12);
        sfx(900, 40);
        break;
    case D_FAST:
        for (auto &b : balls)
            setBSpd(b, std::min(MAXSP, bSpd(b) * 1.25f));
        sfx(850, 40);
        break;
    case D_MULTI:
        addMulti();
        sfx(1300, 70);
        break;
    case D_MAGNET:
        pad.magEnd = gt + 12;
        sfx(1100, 50);
        break;
    case D_SHIELD:
        pad.shield = 1;
        sfx(1000, 50);
        break;
    case D_LASER:
        pad.lasEnd = gt + 15;
        sfx(1350, 50);
        break;
    case D_SLOW:
        slowUntil = gt + 7;
        sfx(700, 80);
        break;
    case D_SHRINK:
        setPadW(75, gt + 10);
        sfx(300, 70);
        break;
    case D_DEATH:
        loseLife();
        break;
    }
}

void launchAll()
{
    for (auto &b : balls)
    {
        if (b.stuck)
        {
            b.stuck = false;
            b.vy = std::fabs(b.vy);
            if (std::fabs(b.vx) < 20)
                b.vx = rnd() < 0.5f ? -150.0f : 150.0f;
        }
    }
    sfx(700, 30);
}

bool anyStuck()
{
    for (auto &b : balls)
        if (b.stuck)
            return true;
    return false;
}

void fireLaser()
{
    if (pad.lasEnd <= gt || gt - pad.lastLas < 0.18f)
        return;
    pad.lastLas = gt;
    Bullet a = {pad.x + 14, pad.y + pad.h, 5, 15, 570, true};
    Bullet b = a;
    b.x = pad.x + pad.w - 19;
    bullets.push_back(a);
    bullets.push_back(b);
    sfx(1700, 18);
}

// ============ UPDATE ============
void updBricks()
{
    for (auto &b : bricks)
    {
        if (!b.alive)
            continue;
        if (b.moving)
        {
            b.x = b.baseX + sin(gt * (b.boss ? 1.1f : 1.8f) + b.phase) * b.amp;
            b.x = clp(b.x, 10, W - b.w - 10);
        }
        if (b.boss && gt > bossDrop)
        {
            DType t = rnd() < 0.55f ? D_DEATH : D_SHRINK;
            mkDrop(b.x + b.w / 2, b.y - 8, t);
            bossDrop = gt + 3 + rnd() * 2;
            note("Boss Attack!", 1);
        }
    }
}

void updParts(float dt)
{
    for (auto &p : parts)
    {
        p.life -= dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.vy -= 160 * dt;
    }
    parts.erase(std::remove_if(parts.begin(), parts.end(),
                               [](const Particle &p)
                               { return p.life <= 0; }),
                parts.end());
}

void updBullets(float dt)
{
    for (auto &bl : bullets)
    {
        if (!bl.active)
            continue;
        bl.y += bl.vy * dt;
        if (bl.y > H)
        {
            bl.active = false;
            continue;
        }
        for (size_t j = 0; j < bricks.size(); j++)
        {
            if (!bricks[j].alive)
                continue;
            if (rOver(bl.x, bl.y, bl.w, bl.h, bricks[j].x, bricks[j].y, bricks[j].w, bricks[j].h))
            {
                dmgBrick(j, 1);
                bl.active = false;
                break;
            }
        }
    }
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
                                 [](const Bullet &b)
                                 { return !b.active; }),
                  bullets.end());
}

void updDrops(float dt)
{
    for (auto &d : drops)
    {
        if (!d.active)
            continue;
        if (pad.magEnd > gt && !badD(d.type))
        {
            float pc = pad.x + pad.w / 2;
            d.x += (pc - d.x) * 2.3f * dt;
        }
        d.y += d.vy * dt;
        if (d.y + d.size < 0)
        {
            d.active = false;
            continue;
        }
        if (rOver(d.x - d.size / 2, d.y - d.size / 2, d.size, d.size, pad.x, pad.y, pad.w, pad.h))
        {
            DType t = d.type;
            d.active = false;
            applyDrop(t);
            if (scr != PLAY)
                return;
        }
    }
    drops.erase(std::remove_if(drops.begin(), drops.end(),
                               [](const Drop &d)
                               { return !d.active; }),
                drops.end());
}

void updBalls(float dt)
{
    float pdt = (slowUntil > gt) ? dt * 0.55f : dt;
    for (auto &ball : balls)
    {
        if (!ball.active)
            continue;
        if (ball.stuck)
        {
            ball.x = pad.x + pad.w / 2;
            ball.y = pad.y + pad.h + ball.r + 1;
            continue;
        }
        float sp = bSpd(ball);
        if (sp < MAXSP)
            setBSpd(ball, std::min(MAXSP, sp * (1 + 0.003f * dt)));

        if (pad.magEnd > gt && ball.vy < 0 && ball.y < 250)
        {
            float pc = pad.x + pad.w / 2;
            ball.vx += (pc - ball.x) * 1.8f * dt;
        }
        ball.px = ball.x;
        ball.py = ball.y;
        ball.x += ball.vx * pdt;
        ball.y += ball.vy * pdt;

        if (ball.x - ball.r < 0)
        {
            ball.x = ball.r;
            ball.vx = std::fabs(ball.vx);
        }
        if (ball.x + ball.r > W)
        {
            ball.x = W - ball.r;
            ball.vx = -std::fabs(ball.vx);
        }
        if (ball.y + ball.r > H)
        {
            ball.y = H - ball.r;
            ball.vy = -std::fabs(ball.vy);
        }

        if (pad.shield > 0 && ball.y - ball.r < 22 && ball.vy < 0)
        {
            ball.y = 22 + ball.r;
            ball.vy = std::fabs(ball.vy);
            pad.shield--;
            note("Shield Saved You!", 1.3f);
            sfx(1100, 60);
        }

        if (ball.y + ball.r < 0)
        {
            ball.active = false;
            continue;
        }

        if (ball.vy < 0 && cRect(ball.x, ball.y, ball.r, pad.x, pad.y, pad.w, pad.h))
        {
            ball.y = pad.y + pad.h + ball.r + 1;
            float pc = pad.x + pad.w / 2;
            float rel = clp((ball.x - pc) / (pad.w / 2), -1, 1);
            float speed = bSpd(ball);
            ball.vx = rel * speed * 0.92f;
            ball.vy = std::sqrt(std::max(80.0f, speed * speed - ball.vx * ball.vx));
            combo = 0;
            sfx(650, 18);
        }

        for (size_t i = 0; i < bricks.size(); i++)
        {
            Brick &br = bricks[i];
            if (!br.alive)
                continue;
            if (!cRect(ball.x, ball.y, ball.r, br.x, br.y, br.w, br.h))
                continue;
            dmgBrick(i, 1);
            float bx = br.x + br.w / 2, by = br.y + br.h / 2;
            float dx = ball.x - bx, dy = ball.y - by;
            float ox = br.w / 2 + ball.r - std::fabs(dx), oy = br.h / 2 + ball.r - std::fabs(dy);
            ball.x = ball.px;
            ball.y = ball.py;
            if (ox < oy)
                ball.vx = -ball.vx;
            else
                ball.vy = -ball.vy;
            break;
        }
    }
    balls.erase(std::remove_if(balls.begin(), balls.end(),
                               [](const Ball &b)
                               { return !b.active; }),
                balls.end());
    if (balls.empty() && scr == PLAY)
        loseLife();
}

void updGame(float dt)
{
    gt += dt;
    if (kL)
        pad.x -= 540 * dt;
    if (kR)
        pad.x += 540 * dt;
    clampPad();
    if (pad.sizeEnd > 0 && gt > pad.sizeEnd)
    {
        setPadW(pad.baseW, 0);
        pad.sizeEnd = 0;
    }
    if (combo > 0 && gt > comboUntil)
        combo = 0;
    updBricks();
    updParts(dt);
    updBalls(dt);
    if (scr != PLAY)
        return;
    updBullets(dt);
    updDrops(dt);
    if (scr != PLAY)
        return;
    if (allGone())
        levelDone();
}

void timer(int)
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - lastMs) / 1000.0f;
    lastMs = now;
    if (dt > 0.05f)
        dt = 0.05f;
    if (scr == PLAY)
        updGame(dt);
    if (scr == LEVELDONE && rt() - ldReal > 3)
        nextLvl();
    glutPostRedisplay();
    glutTimerFunc(16, timer, 0);
}

// ============ RENDERING ============
void drawBG()
{
    float t = rt();
    glBegin(GL_QUADS);
    glColor3f(0.01f, 0.01f, 0.05f);
    glVertex2f(0, 0);
    glVertex2f(W, 0);
    glColor3f(0.04f, 0.02f, 0.14f);
    glVertex2f(W, H);
    glVertex2f(0, H);
    glEnd();
    glPointSize(2);
    glColor3f(0.45f, 0.85f, 1);
    glBegin(GL_POINTS);
    for (int i = 0; i < 120; i++)
    {
        float x = std::fmod(i * 79.0f + sin(t + i) * 10, (float)W);
        float y = std::fmod(i * 137.0f + t * 15, (float)H);
        glVertex2f(x, y);
    }
    glEnd();
    ring(W / 2.0f, H / 2.0f + 30, 190 + sin(t) * 8, C(0.2f, 0.8f, 1), 0.12f);
}

void drawParts()
{
    for (auto &p : parts)
        rectA(p.x, p.y, 4, 4, p.color, p.life / p.maxLife);
}

void drawBrick(const Brick &b)
{
    if (!b.alive)
        return;
    float hr = (float)b.hp / b.maxHp;
    Col cc = C(b.color.r * (0.55f + 0.45f * hr), b.color.g * (0.55f + 0.45f * hr), b.color.b * (0.55f + 0.45f * hr));
    rect(b.x, b.y, b.w, b.h, cc);
    outl(b.x, b.y, b.w, b.h, C(0, 0, 0));
    if (b.boss)
    {
        ctext(b.x + b.w / 2, b.y + 15, "RIFT BOSS", C(1, 1, 1));
        rect(b.x, b.y + b.h + 7, b.w, 6, C(0.15f, 0.02f, 0.05f));
        rect(b.x, b.y + b.h + 7, b.w * hr, 6, C(1, 0.05f, 0.25f));
        outl(b.x, b.y + b.h + 7, b.w, 6, C(1, 1, 1));
    }
    else
    {
        if (b.maxHp > 1)
            ctext(b.x + b.w / 2, b.y + 7, std::to_string(b.hp), C(1, 1, 1), GLUT_BITMAP_8_BY_13);
        if (b.bomb)
            ctext(b.x + b.w / 2, b.y + 6, "B", C(0, 0, 0));
        if (b.moving)
            rect(b.x + 4, b.y + b.h - 4, b.w - 8, 2, C(0, 1, 1));
    }
}

void drawPad()
{
    if (pad.magEnd > gt)
        ring(pad.x + pad.w / 2, pad.y + 12, 72 + sin(rt() * 6) * 5, C(0.1f, 1, 1), 0.45f);
    rect(pad.x, pad.y, pad.w, pad.h, C(0.2f, 0.75f, 1));
    rect(pad.x + 8, pad.y + 5, pad.w - 16, pad.h - 10, C(0.02f, 0.10f, 0.25f));
    outl(pad.x, pad.y, pad.w, pad.h, C(1, 1, 1));
    if (pad.lasEnd > gt)
    {
        rect(pad.x + 12, pad.y + pad.h, 8, 13, C(1, 0.2f, 0.2f));
        rect(pad.x + pad.w - 20, pad.y + pad.h, 8, 13, C(1, 0.2f, 0.2f));
    }
    if (pad.shield > 0)
        rectA(0, 18, W, 5, C(0.2f, 0.45f, 1), 0.85f);
}

void drawBalls()
{
    for (auto &b : balls)
    {
        Col c = slowUntil > gt ? C(0.65f, 0.35f, 1) : C(0.85f, 0.95f, 1);
        circ(b.x, b.y, b.r * 2, c, 0.16f);
        circ(b.x, b.y, b.r, c);
        circ(b.x - 3, b.y + 3, b.r * 0.35f, C(1, 1, 1), 0.9f);
    }
}

void drawDrops()
{
    for (auto &d : drops)
    {
        if (!d.active)
            continue;
        Col dc = dCol(d.type);
        float s = d.size;
        rect(d.x - s / 2, d.y - s / 2, s, s, dc);
        outl(d.x - s / 2, d.y - s / 2, s, s, C(1, 1, 1));
        ctext(d.x, d.y - 5, dLbl(d.type), C(0, 0, 0));
    }
}

void drawBullets()
{
    for (auto &b : bullets)
        if (b.active)
            rect(b.x, b.y, b.w, b.h, C(1, 0.2f, 0.2f));
}

void drawHUD()
{
    rectA(0, H - 52, W, 52, C(0, 0, 0), 0.72f);
    std::ostringstream o;
    o << "Score: " << score << "    High: " << highScore << "    Lives: " << lives
      << "    Level: " << lvl << "    Time: " << ttext(gt) << "    Balls: " << balls.size();
    text(14, H - 22, o.str());

    std::ostringstream eff;
    if (combo > 0)
        eff << "Combo: " << combo << "   ";
    if (pad.magEnd > gt)
        eff << "Magnet ";
    if (pad.lasEnd > gt)
        eff << "Laser ";
    if (slowUntil > gt)
        eff << "Time Rift ";
    if (pad.shield > 0)
        eff << "Shield ";
    std::string ln = eff.str();
    if (ln.empty())
        ln = "Mouse/Arrows: Move | Space: Launch/Laser | P: Pause | M: Menu | Q: Exit";
    text(14, H - 42, ln, C(0.7f, 0.95f, 1), GLUT_BITMAP_8_BY_13);

    if (noticeUntil > gt)
        ctext(W / 2.0f, H - 84, notice, C(1, 0.95f, 0.25f));
}

void drawGame()
{
    drawBG();
    rect(0, 0, 7, H, C(0.25f, 0.25f, 0.38f));
    rect(W - 7, 0, 7, H, C(0.25f, 0.25f, 0.38f));
    rect(0, H - 7, W, 7, C(0.25f, 0.25f, 0.38f));
    for (auto &b : bricks)
        drawBrick(b);
    drawParts();
    drawDrops();
    drawBullets();
    drawPad();
    drawBalls();
    drawHUD();
}

void drawMenu()
{
    drawBG();
    float bw = 320, bh = 42, bx = W / 2.0f - bw / 2, y = 480, gap = 58;
    const char *items[] = {"1. New Game", "2. Resume Game", "3. High Score", "4. Help", "5. Exit"};
    for (int i = 0; i < 5; i++)
        btn(bx, y - i * gap, bw, bh, items[i], i != 1 || hasGame);
    ctext(W / 2.0f, 90, std::string("Sound: ") + (soundOn ? "ON" : "OFF") + " | Press S to toggle",
          C(0.75f, 0.90f, 1), GLUT_BITMAP_8_BY_13);
}

void drawHelp()
{
    drawBG();
    ctext(W / 2.0f, 620, "HELP / CONTROLS", C(0.25f, 0.95f, 1), GLUT_BITMAP_TIMES_ROMAN_24);
    const char *lines[] = {
        "Goal: Break all bricks and clear levels as fast as possible.",
        "Move: Mouse, A/D, or Arrow keys.",
        "Launch: Space or Left Mouse Click.",
        "Laser: Catch Z, then press Space/Click to shoot.",
        "Pause: P     Menu: M/Esc     Exit: Q",
        "",
        "Powerups: L=Life W=Wide F=Fast M=Multi G=Magnet",
        "          D=Shield Z=Laser T=Slow S=Shrink X=Death",
        "",
        "Bonus: Boss levels, moving bricks, bomb bricks, combos, particles."};
    float x = 105, y = 560;
    for (auto &l : lines)
    {
        text(x, y, l);
        y -= 28;
    }
    btn(W / 2.0f - 130, 95, 260, 42, "Back to Menu");
}

void drawHighSc()
{
    drawBG();
    ctext(W / 2.0f, 560, "HIGH SCORE", C(0.25f, 0.95f, 1), GLUT_BITMAP_TIMES_ROMAN_24);
    ctext(W / 2.0f, 500, "Best Score: " + std::to_string(highScore));
    btn(W / 2.0f - 130, 300, 260, 42, "Back to Menu");
}

void drawPause()
{
    rectA(0, 0, W, H, C(0, 0, 0), 0.65f);
    ctext(W / 2.0f, 470, "PAUSED", C(1, 0.9f, 0.25f), GLUT_BITMAP_TIMES_ROMAN_24);
    float bx = W / 2.0f - 130;
    const char *items[] = {"Resume", "Menu", "New Game", "Exit"};
    for (int i = 0; i < 4; i++)
        btn(bx, 385 - i * 55, 260, 42, items[i]);
}

void drawGO()
{
    rectA(0, 0, W, H, C(0, 0, 0), 0.70f);
    ctext(W / 2.0f, 470, "GAME OVER", C(1, 0.2f, 0.25f), GLUT_BITMAP_TIMES_ROMAN_24);
    ctext(W / 2.0f, 430, "Final Score: " + std::to_string(score));
    float bx = W / 2.0f - 130;
    const char *items[] = {"New Game", "Menu", "Exit"};
    for (int i = 0; i < 3; i++)
        btn(bx, 340 - i * 55, 260, 42, items[i]);
}

void drawLD()
{
    rectA(0, 0, W, H, C(0, 0, 0), 0.65f);
    ctext(W / 2.0f, 480, "LEVEL CLEARED!", C(0.2f, 1, 0.35f), GLUT_BITMAP_TIMES_ROMAN_24);
    ctext(W / 2.0f, 440, "Next Level: " + std::to_string(lvl + 1));
    ctext(W / 2.0f, 410, "Score: " + std::to_string(score) + " | Time: " + ttext(gt));
    float bx = W / 2.0f - 130;
    btn(bx, 320, 260, 42, "Next Level Now");
    btn(bx, 265, 260, 42, "Menu");
}

void display()
{
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    if (scr == MENU)
        drawMenu();
    else if (scr == HELP)
        drawHelp();
    else if (scr == HIGHSCORE)
        drawHighSc();
    else
    {
        drawGame();
        if (scr == PAUSE)
            drawPause();
        else if (scr == GAMEOVER)
            drawGO();
        else if (scr == LEVELDONE)
            drawLD();
    }
    glutSwapBuffers();
}

// ============ INPUT ============
void s2w(int mx, int my, float &x, float &y)
{
    x = mx * (float)W / winW;
    y = H - my * (float)H / winH;
}

void movePadM(int mx, int my)
{
    float x, y;
    s2w(mx, my, x, y);
    pad.x = x - pad.w / 2;
    clampPad();
}

void mClick(int b, int a, int mx, int my)
{
    if (b != GLUT_LEFT_BUTTON || a != GLUT_DOWN)
        return;
    float x, y;
    s2w(mx, my, x, y);

    if (scr == PLAY)
    {
        movePadM(mx, my);
        if (anyStuck())
            launchAll();
        else
            fireLaser();
    }
    else if (scr == MENU)
    {
        float bw = 320, bh = 42, bx = W / 2.0f - bw / 2, sy = 480, gap = 58;
        for (int i = 0; i < 5; i++)
        {
            if (!ins(x, y, bx, sy - i * gap, bw, bh))
                continue;
            if (i == 0)
                startNew();
            else if (i == 1 && hasGame)
                resumeG();
            else if (i == 2)
                scr = HIGHSCORE;
            else if (i == 3)
                scr = HELP;
            else if (i == 4)
                exit(0);
        }
    }
    else if (scr == HELP)
    {
        if (ins(x, y, W / 2.0f - 130, 95, 260, 42))
            scr = MENU;
    }
    else if (scr == HIGHSCORE)
    {
        if (ins(x, y, W / 2.0f - 130, 300, 260, 42))
            scr = MENU;
    }
    else if (scr == PAUSE)
    {
        float bx = W / 2.0f - 130;
        if (ins(x, y, bx, 385, 260, 42))
            resumeG();
        else if (ins(x, y, bx, 330, 260, 42))
            scr = MENU;
        else if (ins(x, y, bx, 275, 260, 42))
            startNew();
        else if (ins(x, y, bx, 220, 260, 42))
            exit(0);
    }
    else if (scr == GAMEOVER)
    {
        float bx = W / 2.0f - 130;
        if (ins(x, y, bx, 340, 260, 42))
            startNew();
        else if (ins(x, y, bx, 285, 260, 42))
            scr = MENU;
        else if (ins(x, y, bx, 230, 260, 42))
            exit(0);
    }
    else if (scr == LEVELDONE)
    {
        float bx = W / 2.0f - 130;
        if (ins(x, y, bx, 320, 260, 42))
            nextLvl();
        else if (ins(x, y, bx, 265, 260, 42))
            scr = MENU;
    }
}

void mMove(int mx, int my)
{
    if (scr == PLAY)
        movePadM(mx, my);
}

void kb(unsigned char key, int, int)
{
    char k = tolower(key);
    if (k == 'q')
        exit(0);
    if (k == 's')
    {
        soundOn = !soundOn;
        return;
    }
    if (key == 27)
    {
        if (scr == PLAY)
            scr = PAUSE;
        else if (scr == MENU)
            exit(0);
        else
            scr = MENU;
        return;
    }
    if (scr == PLAY)
    {
        if (k == 'a')
            kL = true;
        if (k == 'd')
            kR = true;
        if (k == 'p')
            scr = PAUSE;
        else if (k == 'm')
            scr = MENU;
        else if (key == ' ')
        {
            if (anyStuck())
                launchAll();
            else
                fireLaser();
        }
    }
    else if (scr == MENU)
    {
        if (k == '1' || k == 'n')
            startNew();
        else if (k == '2' || k == 'r')
            resumeG();
        else if (k == '3')
            scr = HIGHSCORE;
        else if (k == '4' || k == 'h')
            scr = HELP;
        else if (k == '5')
            exit(0);
    }
    else if (scr == PAUSE)
    {
        if (k == 'p' || k == 'r')
            resumeG();
        else if (k == 'm')
            scr = MENU;
        else if (k == 'n')
            startNew();
    }
    else if (scr == HELP || scr == HIGHSCORE)
    {
        if (k == 'b' || k == 'm')
            scr = MENU;
    }
    else if (scr == GAMEOVER)
    {
        if (k == 'n')
            startNew();
        else if (k == 'm')
            scr = MENU;
    }
    else if (scr == LEVELDONE)
    {
        if (k == 'n')
            nextLvl();
        else if (k == 'm')
            scr = MENU;
    }
}

void kbUp(unsigned char key, int, int)
{
    char k = tolower(key);
    if (k == 'a')
        kL = false;
    if (k == 'd')
        kR = false;
}

void spec(int key, int, int)
{
    if (key == GLUT_KEY_F1)
    {
        scr = HELP;
        return;
    }
    if (scr != PLAY)
        return;
    if (key == GLUT_KEY_LEFT)
        kL = true;
    else if (key == GLUT_KEY_RIGHT)
        kR = true;
}

void specUp(int key, int, int)
{
    if (key == GLUT_KEY_LEFT)
        kL = false;
    else if (key == GLUT_KEY_RIGHT)
        kR = false;
}

void reshape(int w, int h)
{
    winW = std::max(1, w);
    winH = std::max(1, h);
    glViewport(0, 0, winW, winH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, W, 0, H);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char **argv)
{
    srand((unsigned)time(NULL));
    loadHigh();
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(W, H);
    glutInitWindowPosition(80, 40);
    glutCreateWindow("DX Ball");
    glClearColor(0.01f, 0.01f, 0.04f, 1);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(kb);
    glutKeyboardUpFunc(kbUp);
    glutSpecialFunc(spec);
    glutSpecialUpFunc(specUp);
    glutMouseFunc(mClick);
    glutMotionFunc(mMove);
    glutPassiveMotionFunc(mMove);
    lastMs = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(16, timer, 0);
    glutMainLoop();
    return 0;
}
