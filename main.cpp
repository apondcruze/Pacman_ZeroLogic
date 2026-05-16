#include <GL/freeglut.h>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cctype>
using namespace std;

const int W = 800;
const int H = 740;
const int ROWS = 19;
const int COLS = 19;
const float CELL = 35.0f;
const float MAZE_H = ROWS * CELL;
const float HUD_Y = MAZE_H + 35.0f;

enum State { MENU, PLAYING, PAUSED, GAME_OVER, GAME_WON };
State state = MENU;
bool gameInProgress = false;
int menuSel = 0;

const int UP=0, DOWN=1, LEFT=2, RIGHT=3;

struct Entity {
    float x,y;
    int dir;
    float speed;
    Entity(float X=0,float Y=0,int D=RIGHT,float S=3.5f)
        : x(X),y(Y),dir(D),speed(S){}
};

struct Ghost {
    float x, y;
    int dir;
    float speed;
    float r, g, b;
    Ghost(float X=0, float Y=0, float R=1, float G=0, float B=0)
        : x(X), y(Y), dir(RIGHT), speed(2.6f), r(R), g(G), b(B) {}
};

bool keys[256]={false};
bool specialKeys[256]={false};

int maze[ROWS][COLS] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,1},
    {1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,1,1,0,1},
    {1,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,1,0,1},
    {1,0,1,1,1,0,1,1,1,0,1,1,1,1,1,0,1,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,0,1},
    {1,1,1,0,1,1,1,1,1,1,1,1,1,0,1,1,1,0,1},
    {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,1,0,1},
    {1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,0,1,0,1},
    {1,0,0,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,0,1,1,1,1,1,0,1,1,1,1,1,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0,0,0,1},
    {1,1,1,0,1,1,1,1,1,1,1,0,1,1,1,0,1,1,1},
    {1,0,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1},
    {1,0,1,1,1,1,1,1,1,0,1,1,1,0,1,1,1,0,1},
    {1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1},
    {1,0,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,0,1},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

Entity pacman;
float mouthAnim=20;
bool opening=true;

Ghost ghosts[4];
const float BASE_GHOST_SPEED = 2.6f;

bool dots[ROWS][COLS];
int dotsRemaining = 0;

int score = 0;
int lives = 3;
int startTimeMs = 0;
int pauseAccumMs = 0;
int pauseStartMs = 0;
int elapsedSec = 0;

bool canMove(float x,float y){
    const float r=11;
    int l=(x-r)/CELL, rgt=(x+r)/CELL;
    int b=(y-r)/CELL, t=(y+r)/CELL;
    if(l<0||rgt>=COLS||b<0||t>=ROWS) return false;
    return !(maze[ROWS-1-t][l]||maze[ROWS-1-t][rgt]||
             maze[ROWS-1-b][l]||maze[ROWS-1-b][rgt]);
}

int chooseGhostDir(Ghost& g, float pacX, float pacY) {
    float distToPac = hypot(g.x - pacX, g.y - pacY);
    bool shouldChase = (distToPac < 130.0f);

    int possible[4] = {-1,-1,-1,-1};
    int count = 0;
    float px = g.x, py = g.y;

    for(int d=0; d<4; d++) {
        if(d == (g.dir + 2) % 4) continue;
        float dx=0, dy=0;
        if(d==LEFT)  dx = -g.speed;
        if(d==RIGHT) dx =  g.speed;
        if(d==UP)    dy =  g.speed;
        if(d==DOWN)  dy = -g.speed;
        if(canMove(px + dx, py + dy)) possible[count++] = d;
    }

    if(count == 0) return g.dir;

    if(shouldChase) {
        int best = possible[0];
        float bestDist = 1e9;
        for(int i=0; i<count; i++) {
            int d = possible[i];
            float dx=0, dy=0;
            if(d==LEFT)  dx = -g.speed*3.0f;
            if(d==RIGHT) dx =  g.speed*3.0f;
            if(d==UP)    dy =  g.speed*3.0f;
            if(d==DOWN)  dy = -g.speed*3.0f;
            float newx = px + dx, newy = py + dy;
            float dist = (newx - pacX)*(newx - pacX) + (newy - pacY)*(newy - pacY);
            if(dist < bestDist) { bestDist = dist; best = d; }
        }
        return best;
    } else {
        for(int i=0; i<count; i++)
            if(possible[i] == g.dir) return g.dir;
        return possible[rand() % count];
    }
}

void placePacmanSpawn(){
    pacman = Entity(1.5f*CELL, (ROWS-1.5f)*CELL, RIGHT, 3.0f);
}

void placeGhostSpawns(){
    ghosts[0] = Ghost(3.5f*CELL,  15.5f*CELL, 1.0f, 0.0f, 0.0f);
    ghosts[1] = Ghost(15.5f*CELL, 15.5f*CELL, 1.0f, 0.6f, 0.6f);
    ghosts[2] = Ghost(5.5f*CELL,   5.5f*CELL, 0.0f, 1.0f, 1.0f);
    ghosts[3] = Ghost(13.5f*CELL,  8.5f*CELL, 1.0f, 0.6f, 0.0f);
}

bool isSpawnCell(int row, int col){
    // Pacman spawn cell
    if(row == 1 && col == 1) return true;
    // Ghost spawn cells
    if(row == 3 && col == 3) return true;
    if(row == 3 && col == 15) return true;
    if(row == 13 && col == 5) return true;
    if(row == 10 && col == 13) return true;
    return false;
}

void resetDots(){
    dotsRemaining = 0;
    for(int i=0;i<ROWS;i++){
        for(int j=0;j<COLS;j++){
            if(maze[i][j]==0 && !isSpawnCell(i,j)){
                dots[i][j] = true;
                dotsRemaining++;
            } else {
                dots[i][j] = false;
            }
        }
    }
}

void resetGame(){
    resetDots();
    score = 0;
    lives = 3;
    placePacmanSpawn();
    placeGhostSpawns();
    startTimeMs = glutGet(GLUT_ELAPSED_TIME);
    pauseAccumMs = 0;
    elapsedSec = 0;
}

void respawnAfterDeath(){
    placePacmanSpawn();
    placeGhostSpawns();
}

void drawMaze(){
    glColor3f(0,0,1);
    glLineWidth(4);
    for(int i=0;i<ROWS;i++){
        for(int j=0;j<COLS;j++){
            if(maze[i][j]==1){
                float x=j*CELL;
                float y=(ROWS-1-i)*CELL;
                if(i==0||maze[i-1][j]==0){
                    glBegin(GL_LINES); glVertex2f(x,y+CELL); glVertex2f(x+CELL,y+CELL); glEnd();
                }
                if(i==ROWS-1||maze[i+1][j]==0){
                    glBegin(GL_LINES); glVertex2f(x,y); glVertex2f(x+CELL,y); glEnd();
                }
                if(j==0||maze[i][j-1]==0){
                    glBegin(GL_LINES); glVertex2f(x,y); glVertex2f(x,y+CELL); glEnd();
                }
                if(j==COLS-1||maze[i][j+1]==0){
                    glBegin(GL_LINES); glVertex2f(x+CELL,y); glVertex2f(x+CELL,y+CELL); glEnd();
                }
            }
        }
    }
}

void drawDots(){
    glColor3f(1.0f, 0.85f, 0.5f);
    for(int i=0;i<ROWS;i++){
        for(int j=0;j<COLS;j++){
            if(!dots[i][j]) continue;
            float cx = (j+0.5f)*CELL;
            float cy = (ROWS-1-i+0.5f)*CELL;
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(cx, cy);
            for(int a=0;a<=360;a+=30){
                float r = a*3.1416f/180;
                glVertex2f(cx+3.0f*cos(r), cy+3.0f*sin(r));
            }
            glEnd();
        }
    }
}

void drawPacman(){
    glColor3f(1,1,0);
    float s=0,e=0;
    switch(pacman.dir){
        case RIGHT: s=mouthAnim; e=360-mouthAnim; break;
        case LEFT: s=180+mouthAnim; e=180-mouthAnim+360; break;
        case UP: s=90+mouthAnim; e=90-mouthAnim+360; break;
        case DOWN: s=270+mouthAnim; e=270-mouthAnim+360; break;
    }
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(pacman.x,pacman.y);
    for(float a=s;a<=e;a+=5){
        float r=a*3.1416f/180;
        glVertex2f(pacman.x+13*cos(r),pacman.y+13*sin(r));
    }
    glEnd();

    float ex=pacman.x, ey=pacman.y+6;
    if(pacman.dir==RIGHT) ex+=5;
    if(pacman.dir==LEFT) ex-=5;
    if(pacman.dir==UP) ey+=2;
    if(pacman.dir==DOWN) ey-=2;
    glColor3f(0,0,0);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(ex,ey);
    for(int i=0;i<=360;i+=10){
        float r=i*3.1416f/180;
        glVertex2f(ex+2.5*cos(r),ey+2.5*sin(r));
    }
    glEnd();
}

void drawGhost(const Ghost& g){
    glColor3f(g.r, g.g, g.b);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(g.x, g.y+4);
    for(float a=0; a<=360; a+=8){
        float r = a * 3.1416f / 180;
        glVertex2f(g.x + 14*cos(r), g.y + 4 + 13*sin(r));
    }
    glEnd();

    glColor3f(0,0,0);
    glBegin(GL_LINES);
    for(int i=-13; i<=13; i+=7){
        glVertex2f(g.x+i, g.y-9);
        glVertex2f(g.x+i+4, g.y-14);
    }
    glEnd();

    glColor3f(1,1,1);
    glBegin(GL_TRIANGLE_FAN); glVertex2f(g.x-5.5, g.y+6); for(int i=0;i<=360;i+=15) glVertex2f(g.x-5.5+4.5*cos(i*3.1416f/180), g.y+6+4.5*sin(i*3.1416f/180)); glEnd();
    glBegin(GL_TRIANGLE_FAN); glVertex2f(g.x+5.5, g.y+6); for(int i=0;i<=360;i+=15) glVertex2f(g.x+5.5+4.5*cos(i*3.1416f/180), g.y+6+4.5*sin(i*3.1416f/180)); glEnd();

    glColor3f(0,0,0);
    float eyeOff = (g.dir == LEFT) ? -1.8f : (g.dir == RIGHT) ? 1.8f : 0.0f;
    glBegin(GL_TRIANGLE_FAN); glVertex2f(g.x-5.5+eyeOff, g.y+6); for(int i=0;i<=360;i+=30) glVertex2f(g.x-5.5+eyeOff+2*cos(i*3.1416f/180), g.y+6+2*sin(i*3.1416f/180)); glEnd();
    glBegin(GL_TRIANGLE_FAN); glVertex2f(g.x+5.5+eyeOff, g.y+6); for(int i=0;i<=360;i+=30) glVertex2f(g.x+5.5+eyeOff+2*cos(i*3.1416f/180), g.y+6+2*sin(i*3.1416f/180)); glEnd();
}

void drawText(float x, float y, void* font, const char* s){
    glRasterPos2f(x, y);
    for(int i=0;s[i];i++) glutBitmapCharacter(font, s[i]);
}

int textWidth(void* font, const char* s){
    int w = 0;
    for(int i=0;s[i];i++) w += glutBitmapWidth(font, s[i]);
    return w;
}

void drawHUD(){
    char buf[64];
    glColor3f(1,1,1);
    sprintf(buf, "Score: %04d", score);
    drawText(15, HUD_Y, GLUT_BITMAP_HELVETICA_18, buf);

    int mm = elapsedSec / 60, ss = elapsedSec % 60;
    sprintf(buf, "Time: %02d:%02d", mm, ss);
    int tw = textWidth(GLUT_BITMAP_HELVETICA_18, buf);
    drawText(W/2 - tw/2, HUD_Y, GLUT_BITMAP_HELVETICA_18, buf);

    sprintf(buf, "Lives: %d", lives);
    int lw = textWidth(GLUT_BITMAP_HELVETICA_18, buf);
    drawText(W - lw - 15, HUD_Y, GLUT_BITMAP_HELVETICA_18, buf);

    glColor3f(0.5f, 0.5f, 0.5f);
    drawText(15, HUD_Y - 20, GLUT_BITMAP_HELVETICA_12, "P: Pause   ESC: Menu");
}

void drawCenteredOverlay(const char* line1, const char* line2){
    glColor4f(0,0,0,0.6f);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glBegin(GL_QUADS);
    glVertex2f(W/2 - 200, H/2 - 60);
    glVertex2f(W/2 + 200, H/2 - 60);
    glVertex2f(W/2 + 200, H/2 + 60);
    glVertex2f(W/2 - 200, H/2 + 60);
    glEnd();
    glDisable(GL_BLEND);

    glColor3f(1,1,0);
    int w1 = textWidth(GLUT_BITMAP_TIMES_ROMAN_24, line1);
    drawText(W/2 - w1/2, H/2 + 10, GLUT_BITMAP_TIMES_ROMAN_24, line1);
    glColor3f(1,1,1);
    int w2 = textWidth(GLUT_BITMAP_HELVETICA_18, line2);
    drawText(W/2 - w2/2, H/2 - 25, GLUT_BITMAP_HELVETICA_18, line2);
}

void drawMenu(){
    glColor3f(1,1,0);
    const char* title = "PAC - MAN";
    int tw = textWidth(GLUT_BITMAP_TIMES_ROMAN_24, title);
    drawText(W/2 - tw/2, H - 150, GLUT_BITMAP_TIMES_ROMAN_24, title);

    const char* items[3] = {"Start", "Resume", "Exit"};
    for(int i=0;i<3;i++){
        bool disabled = (i==1 && !gameInProgress);
        if(disabled) glColor3f(0.4f, 0.4f, 0.4f);
        else if(i == menuSel) glColor3f(1.0f, 0.6f, 0.0f);
        else glColor3f(1,1,1);

        char buf[64];
        sprintf(buf, "%s %s", (i==menuSel ? ">" : " "), items[i]);
        int w = textWidth(GLUT_BITMAP_HELVETICA_18, buf);
        drawText(W/2 - w/2, H/2 + 40 - i*40, GLUT_BITMAP_HELVETICA_18, buf);
    }

    glColor3f(0.6f, 0.6f, 0.6f);
    const char* help = "Up/Down to navigate, Enter to select";
    int hw = textWidth(GLUT_BITMAP_HELVETICA_12, help);
    drawText(W/2 - hw/2, 80, GLUT_BITMAP_HELVETICA_12, help);
}

void updateElapsed(){
    int now = glutGet(GLUT_ELAPSED_TIME);
    elapsedSec = (now - startTimeMs - pauseAccumMs) / 1000;
}

void applyGhostSpeed(){
    float mul = 1.0f + 0.04f * (elapsedSec / 10);
    if(mul > 2.0f) mul = 2.0f;
    for(int i=0;i<4;i++) ghosts[i].speed = BASE_GHOST_SPEED * mul;
}

void eatDotIfAny(){
    int col = (int)(pacman.x / CELL);
    int row = ROWS - 1 - (int)(pacman.y / CELL);
    if(row<0||row>=ROWS||col<0||col>=COLS) return;
    if(dots[row][col]){
        dots[row][col] = false;
        dotsRemaining--;
        score += 10;
        if(dotsRemaining == 0){
            state = GAME_WON;
            gameInProgress = false;
        }
    }
}

void update(int value){
    if(state==PLAYING){
        updateElapsed();
        applyGhostSpeed();

        if(opening) mouthAnim+=2; else mouthAnim-=2;
        if(mouthAnim>40) opening=false;
        if(mouthAnim<10) opening=true;

        float dx=0,dy=0;
        if(keys['a']||specialKeys[GLUT_KEY_LEFT]) {dx=-pacman.speed; pacman.dir=LEFT;}
        else if(keys['d']||specialKeys[GLUT_KEY_RIGHT]) {dx=pacman.speed; pacman.dir=RIGHT;}
        else if(keys['w']||specialKeys[GLUT_KEY_UP]) {dy=pacman.speed; pacman.dir=UP;}
        else if(keys['s']||specialKeys[GLUT_KEY_DOWN]) {dy=-pacman.speed; pacman.dir=DOWN;}

        if(canMove(pacman.x+dx, pacman.y+dy)){
            pacman.x+=dx;
            pacman.y+=dy;
        }

        eatDotIfAny();
        if(state != PLAYING){ glutPostRedisplay(); glutTimerFunc(16,update,0); return; }

        for(int i=0; i<4; i++){
            Ghost& g = ghosts[i];
            int newDir = chooseGhostDir(g, pacman.x, pacman.y);
            g.dir = newDir;

            float gx=0, gy=0;
            if(newDir==LEFT)  gx = -g.speed;
            if(newDir==RIGHT) gx =  g.speed;
            if(newDir==UP)    gy =  g.speed;
            if(newDir==DOWN)  gy = -g.speed;

            if(canMove(g.x + gx, g.y + gy)){
                g.x += gx;
                g.y += gy;
            } else {
                for(int d=0; d<4; d++){
                    if(d == (newDir + 2)%4) continue;
                    float tx=0, ty=0;
                    if(d==LEFT) tx=-g.speed;
                    if(d==RIGHT) tx=g.speed;
                    if(d==UP) ty=g.speed;
                    if(d==DOWN) ty=-g.speed;
                    if(canMove(g.x+tx, g.y+ty)){
                        g.dir = d;
                        g.x += tx;
                        g.y += ty;
                        break;
                    }
                }
            }

            float dist = hypot(g.x - pacman.x, g.y - pacman.y);
            if(dist < 23){
                lives--;
                if(lives <= 0){
                    state = GAME_OVER;
                    gameInProgress = false;
                } else {
                    respawnAfterDeath();
                }
                break;
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void display(){
    glClear(GL_COLOR_BUFFER_BIT);

    if(state == MENU){
        drawMenu();
    } else {
        drawMaze();
        drawDots();
        drawPacman();
        for(int i=0; i<4; i++) drawGhost(ghosts[i]);
        drawHUD();

        if(state == PAUSED){
            drawCenteredOverlay("PAUSED", "Press P to resume, ESC for Menu");
        } else if(state == GAME_OVER){
            char buf[64];
            sprintf(buf, "Final Score: %d  -  Press M for Menu", score);
            drawCenteredOverlay("GAME OVER", buf);
        } else if(state == GAME_WON){
            char buf[64];
            sprintf(buf, "Score: %d  Time: %02d:%02d  -  Press M for Menu",
                    score, elapsedSec/60, elapsedSec%60);
            drawCenteredOverlay("YOU WIN!", buf);
        }
    }

    glutSwapBuffers();
}

void menuConfirm(){
    if(menuSel == 0){
        resetGame();
        state = PLAYING;
        gameInProgress = true;
    } else if(menuSel == 1){
        if(gameInProgress){
            pauseAccumMs += glutGet(GLUT_ELAPSED_TIME) - pauseStartMs;
            state = PLAYING;
        }
    } else if(menuSel == 2){
        exit(0);
    }
}

void keyDown(unsigned char k,int,int){
    if(k == 27){ // ESC
        if(state == PLAYING){
            pauseStartMs = glutGet(GLUT_ELAPSED_TIME);
            state = MENU;
            menuSel = gameInProgress ? 1 : 0;
        } else if(state == PAUSED){
            state = MENU;
            menuSel = gameInProgress ? 1 : 0;
        } else if(state == MENU){
            exit(0);
        }
        return;
    }

    if(k == 13 || k == ' '){ // Enter or Space
        if(state == MENU){
            // Skip Resume if disabled
            if(menuSel == 1 && !gameInProgress) return;
            menuConfirm();
            return;
        }
    }

    k = tolower(k);
    keys[k] = true;

    if(state == MENU){
        if(k == 'w'){
            menuSel = (menuSel + 2) % 3;
            if(menuSel == 1 && !gameInProgress) menuSel = (menuSel + 2) % 3;
        } else if(k == 's'){
            menuSel = (menuSel + 1) % 3;
            if(menuSel == 1 && !gameInProgress) menuSel = (menuSel + 1) % 3;
        }
        return;
    }

    if(k == 'p'){
        if(state == PLAYING){
            state = PAUSED;
            pauseStartMs = glutGet(GLUT_ELAPSED_TIME);
        } else if(state == PAUSED){
            pauseAccumMs += glutGet(GLUT_ELAPSED_TIME) - pauseStartMs;
            state = PLAYING;
        }
        return;
    }

    if(k == 'm' && (state == GAME_OVER || state == GAME_WON)){
        state = MENU;
        menuSel = 0;
        return;
    }
}

void keyUp(unsigned char k,int,int){
    k=tolower(k);
    keys[k]=false;
}

void specialDown(int k,int,int){
    specialKeys[k]=true;
    if(state == MENU){
        if(k == GLUT_KEY_UP){
            menuSel = (menuSel + 2) % 3;
            if(menuSel == 1 && !gameInProgress) menuSel = (menuSel + 2) % 3;
        } else if(k == GLUT_KEY_DOWN){
            menuSel = (menuSel + 1) % 3;
            if(menuSel == 1 && !gameInProgress) menuSel = (menuSel + 1) % 3;
        }
    }
}

void specialUp(int k,int,int){ specialKeys[k]=false; }

int main(int argc,char** argv){
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);
    glutInitWindowSize(W,H);
    glutCreateWindow("Pacman");
    gluOrtho2D(0,W,0,H);

    glutDisplayFunc(display);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutTimerFunc(0,update,0);

    srand(time(0));

    placePacmanSpawn();
    placeGhostSpawns();
    resetDots();

    glutMainLoop();
    return 0;
}
