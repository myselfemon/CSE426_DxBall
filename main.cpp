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

enum Screen { MENU, PLAY, PAUSE, HELP, HIGHSCORE, GAMEOVER, LEVELDONE };
enum DType { D_LIFE, D_WIDE, D_FAST, D_MULTI, D_MAGNET, D_SHIELD, D_LASER, D_SLOW, D_SHRINK, D_DEATH };

struct Col { float r, g, b; };
struct Paddle { float x, y, w, h, baseW, sizeEnd, magEnd, lasEnd, lastLas; int shield; };
struct Ball { float x, y, px, py, r, vx, vy; bool stuck, active; };
struct Brick { float x, y, baseX, w, h; int hp, maxHp; bool alive, bomb, boss, moving; float phase, amp; Col color; };
struct Drop { float x, y, vy, size; DType type; bool active; };
struct Bullet { float x, y, w, h, vy; bool active; };
struct Particle { float x, y, vx, vy, life, maxLife; Col color; };

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
Col C(float r, float g, float b) { Col c = {r, g, b}; return c; }
float rnd() { return (float)rand() / RAND_MAX; }
float clp(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
float rt() { return glutGet(GLUT_ELAPSED_TIME) / 1000.0f; }

std::string ttext(float t) {
    std::ostringstream o;
    o << (int)t/60 << ":" << std::setw(2) << std::setfill('0') << (int)t%60;
    return o.str();
}



void drawBrick(const Brick& b) {
    if (!b.alive) return;
    float hr = (float)b.hp / b.maxHp;
    Col cc = C(b.color.r*(0.55f+0.45f*hr), b.color.g*(0.55f+0.45f*hr), b.color.b*(0.55f+0.45f*hr));
    rect(b.x, b.y, b.w, b.h, cc);
    outl(b.x, b.y, b.w, b.h, C(0, 0, 0));
    if (b.boss) {
        ctext(b.x + b.w/2, b.y + 15, "RIFT BOSS", C(1,1,1));
        rect(b.x, b.y+b.h+7, b.w, 6, C(0.15f, 0.02f, 0.05f));
        rect(b.x, b.y+b.h+7, b.w*hr, 6, C(1, 0.05f, 0.25f));
        outl(b.x, b.y+b.h+7, b.w, 6, C(1, 1, 1));
    } else {
        if (b.maxHp > 1) ctext(b.x+b.w/2, b.y+7, std::to_string(b.hp), C(1,1,1), GLUT_BITMAP_8_BY_13);
        if (b.bomb) ctext(b.x+b.w/2, b.y+6, "B", C(0,0,0));
        if (b.moving) rect(b.x+4, b.y+b.h-4, b.w-8, 2, C(0, 1, 1));
    }
}

void drawPad() {
    if (pad.magEnd > gt)
        ring(pad.x+pad.w/2, pad.y+12, 72 + sin(rt()*6)*5, C(0.1f, 1, 1), 0.45f);
    rect(pad.x, pad.y, pad.w, pad.h, C(0.2f, 0.75f, 1));
    rect(pad.x+8, pad.y+5, pad.w-16, pad.h-10, C(0.02f, 0.10f, 0.25f));
    outl(pad.x, pad.y, pad.w, pad.h, C(1,1,1));
    if (pad.lasEnd > gt) {
        rect(pad.x+12, pad.y+pad.h, 8, 13, C(1, 0.2f, 0.2f));
        rect(pad.x+pad.w-20, pad.y+pad.h, 8, 13, C(1, 0.2f, 0.2f));
    }
    if (pad.shield > 0) rectA(0, 18, W, 5, C(0.2f, 0.45f, 1), 0.85f);
}

void drawBalls() {
    for (auto& b : balls) {
        Col c = slowUntil > gt ? C(0.65f, 0.35f, 1) : C(0.85f, 0.95f, 1);
        circ(b.x, b.y, b.r*2, c, 0.16f);
        circ(b.x, b.y, b.r, c);
        circ(b.x-3, b.y+3, b.r*0.35f, C(1,1,1), 0.9f);
    }
}

void drawDrops() {
    for (auto& d : drops) {
        if (!d.active) continue;
        Col dc = dCol(d.type); float s = d.size;
        rect(d.x-s/2, d.y-s/2, s, s, dc);
        outl(d.x-s/2, d.y-s/2, s, s, C(1,1,1));
        ctext(d.x, d.y-5, dLbl(d.type), C(0,0,0));
    }
}

void drawBullets() {
    for (auto& b : bullets)
        if (b.active) rect(b.x, b.y, b.w, b.h, C(1, 0.2f, 0.2f));
}

void drawHUD() {
    rectA(0, H-52, W, 52, C(0,0,0), 0.72f);
    std::ostringstream o;
    o << "Score: " << score << "    High: " << highScore << "    Lives: " << lives
      << "    Level: " << lvl << "    Time: " << ttext(gt) << "    Balls: " << balls.size();
    text(14, H-22, o.str());

    std::ostringstream eff;
    if (combo > 0) eff << "Combo: " << combo << "   ";
    if (pad.magEnd > gt) eff << "Magnet ";
    if (pad.lasEnd > gt) eff << "Laser ";
    if (slowUntil > gt) eff << "Time Rift ";
    if (pad.shield > 0) eff << "Shield ";
    std::string ln = eff.str();
    if (ln.empty()) ln = "Mouse/Arrows: Move | Space: Launch/Laser | P: Pause | M: Menu | Q: Exit";
    text(14, H-42, ln, C(0.7f, 0.95f, 1), GLUT_BITMAP_8_BY_13);

    if (noticeUntil > gt) ctext(W/2.0f, H-84, notice, C(1, 0.95f, 0.25f));
}

void drawGame() {
    drawBG();
    rect(0, 0, 7, H, C(0.25f, 0.25f, 0.38f));
    rect(W-7, 0, 7, H, C(0.25f, 0.25f, 0.38f));
    rect(0, H-7, W, 7, C(0.25f, 0.25f, 0.38f));
    for (auto& b : bricks) drawBrick(b);
    drawParts(); drawDrops(); drawBullets(); drawPad(); drawBalls(); drawHUD();
}

void drawMenu() {
    drawBG();
    float bw = 320, bh = 42, bx = W/2.0f - bw/2, y = 480, gap = 58;
    const char* items[] = {"1. New Game","2. Resume Game","3. High Score","4. Help","5. Exit"};
    for (int i = 0; i < 5; i++)
        btn(bx, y - i*gap, bw, bh, items[i], i != 1 || hasGame);
    ctext(W/2.0f, 90, std::string("Sound: ") + (soundOn?"ON":"OFF") + " | Press S to toggle",
          C(0.75f, 0.90f, 1), GLUT_BITMAP_8_BY_13);
}

void drawHelp() {
    drawBG();
    ctext(W/2.0f, 620, "HELP / CONTROLS", C(0.25f, 0.95f, 1), GLUT_BITMAP_TIMES_ROMAN_24);
    const char* lines[] = {
        "Goal: Break all bricks and clear levels as fast as possible.",
        "Move: Mouse, A/D, or Arrow keys.",
        "Launch: Space or Left Mouse Click.",
        "Laser: Catch Z, then press Space/Click to shoot.",
        "Pause: P     Menu: M/Esc     Exit: Q",
        "",
        "Powerups: L=Life W=Wide F=Fast M=Multi G=Magnet",
        "          D=Shield Z=Laser T=Slow S=Shrink X=Death",
        "",
        "Bonus: Boss levels, moving bricks, bomb bricks, combos, particles."
    };
    float x = 105, y = 560;
    for (auto& l : lines) { text(x, y, l); y -= 28; }
    btn(W/2.0f - 130, 95, 260, 42, "Back to Menu");
}

void drawHighSc() {
    drawBG();
    ctext(W/2.0f, 560, "HIGH SCORE", C(0.25f, 0.95f, 1), GLUT_BITMAP_TIMES_ROMAN_24);
    ctext(W/2.0f, 500, "Best Score: " + std::to_string(highScore));
    btn(W/2.0f - 130, 300, 260, 42, "Back to Menu");
}

void drawPause() {
    rectA(0, 0, W, H, C(0,0,0), 0.65f);
    ctext(W/2.0f, 470, "PAUSED", C(1, 0.9f, 0.25f), GLUT_BITMAP_TIMES_ROMAN_24);
    float bx = W/2.0f - 130;
    const char* items[] = {"Resume","Menu","New Game","Exit"};
    for (int i = 0; i < 4; i++) btn(bx, 385 - i*55, 260, 42, items[i]);
}

void drawGO() {
    rectA(0, 0, W, H, C(0,0,0), 0.70f);
    ctext(W/2.0f, 470, "GAME OVER", C(1, 0.2f, 0.25f), GLUT_BITMAP_TIMES_ROMAN_24);
    ctext(W/2.0f, 430, "Final Score: " + std::to_string(score));
    float bx = W/2.0f - 130;
    const char* items[] = {"New Game","Menu","Exit"};
    for (int i = 0; i < 3; i++) btn(bx, 340 - i*55, 260, 42, items[i]);
}

void drawLD() {
    rectA(0, 0, W, H, C(0,0,0), 0.65f);
    ctext(W/2.0f, 480, "LEVEL CLEARED!", C(0.2f, 1, 0.35f), GLUT_BITMAP_TIMES_ROMAN_24);
    ctext(W/2.0f, 440, "Next Level: " + std::to_string(lvl+1));
    ctext(W/2.0f, 410, "Score: " + std::to_string(score) + " | Time: " + ttext(gt));
    float bx = W/2.0f - 130;
    btn(bx, 320, 260, 42, "Next Level Now");
    btn(bx, 265, 260, 42, "Menu");
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();
    if (scr == MENU) drawMenu();
    else if (scr == HELP) drawHelp();
    else if (scr == HIGHSCORE) drawHighSc();
    else {
        drawGame();
        if (scr == PAUSE) drawPause();
        else if (scr == GAMEOVER) drawGO();
        else if (scr == LEVELDONE) drawLD();
    }
    glutSwapBuffers();
}

// ============ INPUT ============
void s2w(int mx, int my, float& x, float& y) {
    x = mx * (float)W / winW; y = H - my * (float)H / winH;
}

void movePadM(int mx, int my) {
    float x, y; s2w(mx, my, x, y);
    pad.x = x - pad.w/2; clampPad();
}

void mClick(int b, int a, int mx, int my) {
    if (b != GLUT_LEFT_BUTTON || a != GLUT_DOWN) return;
    float x, y; s2w(mx, my, x, y);

    if (scr == PLAY) {
        movePadM(mx, my);
        if (anyStuck()) launchAll(); else fireLaser();
    } else if (scr == MENU) {
        float bw = 320, bh = 42, bx = W/2.0f - bw/2, sy = 480, gap = 58;
        for (int i = 0; i < 5; i++) {
            if (!ins(x, y, bx, sy - i*gap, bw, bh)) continue;
            if (i == 0) startNew();
            else if (i == 1 && hasGame) resumeG();
            else if (i == 2) scr = HIGHSCORE;
            else if (i == 3) scr = HELP;
            else if (i == 4) exit(0);
        }
    } else if (scr == HELP) {
        if (ins(x, y, W/2.0f-130, 95, 260, 42)) scr = MENU;
    } else if (scr == HIGHSCORE) {
        if (ins(x, y, W/2.0f-130, 300, 260, 42)) scr = MENU;
    } else if (scr == PAUSE) {
        float bx = W/2.0f - 130;
        if (ins(x, y, bx, 385, 260, 42)) resumeG();
        else if (ins(x, y, bx, 330, 260, 42)) scr = MENU;
        else if (ins(x, y, bx, 275, 260, 42)) startNew();
        else if (ins(x, y, bx, 220, 260, 42)) exit(0);
    } else if (scr == GAMEOVER) {
        float bx = W/2.0f - 130;
        if (ins(x, y, bx, 340, 260, 42)) startNew();
        else if (ins(x, y, bx, 285, 260, 42)) scr = MENU;
        else if (ins(x, y, bx, 230, 260, 42)) exit(0);
    } else if (scr == LEVELDONE) {
        float bx = W/2.0f - 130;
        if (ins(x, y, bx, 320, 260, 42)) nextLvl();
        else if (ins(x, y, bx, 265, 260, 42)) scr = MENU;
    }
}

void mMove(int mx, int my) { if (scr == PLAY) movePadM(mx, my); }

void kb(unsigned char key, int, int) {
    char k = tolower(key);
    if (k == 'q') exit(0);
    if (k == 's') { soundOn = !soundOn; return; }
    if (key == 27) {
        if (scr == PLAY) scr = PAUSE;
        else if (scr == MENU) exit(0);
        else scr = MENU;
        return;
    }
    if (scr == PLAY) {
        if (k == 'a') kL = true;
        if (k == 'd') kR = true;
        if (k == 'p') scr = PAUSE;
        else if (k == 'm') scr = MENU;
        else if (key == ' ') { if (anyStuck()) launchAll(); else fireLaser(); }
    } else if (scr == MENU) {
        if (k == '1' || k == 'n') startNew();
        else if (k == '2' || k == 'r') resumeG();
        else if (k == '3') scr = HIGHSCORE;
        else if (k == '4' || k == 'h') scr = HELP;
        else if (k == '5') exit(0);
    } else if (scr == PAUSE) {
        if (k == 'p' || k == 'r') resumeG();
        else if (k == 'm') scr = MENU;
        else if (k == 'n') startNew();
    } else if (scr == HELP || scr == HIGHSCORE) {
        if (k == 'b' || k == 'm') scr = MENU;
    } else if (scr == GAMEOVER) {
        if (k == 'n') startNew();
        else if (k == 'm') scr = MENU;
    } else if (scr == LEVELDONE) {
        if (k == 'n') nextLvl();
        else if (k == 'm') scr = MENU;
    }
}

void kbUp(unsigned char key, int, int) {
    char k = tolower(key);
    if (k == 'a') kL = false;
    if (k == 'd') kR = false;
}

void spec(int key, int, int) {
    if (key == GLUT_KEY_F1) { scr = HELP; return; }
    if (scr != PLAY) return;
    if (key == GLUT_KEY_LEFT) kL = true;
    else if (key == GLUT_KEY_RIGHT) kR = true;
}

void specUp(int key, int, int) {
    if (key == GLUT_KEY_LEFT) kL = false;
    else if (key == GLUT_KEY_RIGHT) kR = false;
}

void reshape(int w, int h) {
    winW = std::max(1, w); winH = std::max(1, h);
    glViewport(0, 0, winW, winH);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluOrtho2D(0, W, 0, H);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}

int main(int argc, char** argv) {
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
