#include <GL/freeglut.h>
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cctype>
using namespace std;

const int W = 800;
const int H = 700;
const int ROWS = 19;
const int COLS = 19;
const float CELL = 35.0f;

enum State { MENU, PLAYING };
State state = MENU;

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

bool canMove(float x,float y){
    const float r=11;
    int l=(x-r)/CELL, rgt=(x+r)/CELL;
    int b=(y-r)/CELL, t=(y+r)/CELL;
    if(l<0||rgt>=COLS||b<0||t>=ROWS) return false;
    return !(maze[ROWS-1-t][l]||maze[ROWS-1-t][rgt]||
             maze[ROWS-1-b][l]||maze[ROWS-1-b][rgt]);
}

// Improved Ghost AI: Roam widely when far, Chase when close
int chooseGhostDir(Ghost& g, float pacX, float pacY) {
    float distToPac = hypot(g.x - pacX, g.y - pacY);
    bool shouldChase = (distToPac < 130.0f);   // Chase range

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

        if(canMove(px + dx, py + dy)) {
            possible[count++] = d;
        }
    }

    if(count == 0) return g.dir;

    if(shouldChase) {
        // Chase Pacman
        int best = possible[0];
        float bestDist = 1e9;
        for(int i=0; i<count; i++) {
            int d = possible[i];
            float dx=0, dy=0;
            if(d==LEFT)  dx = -g.speed*3.0f;
            if(d==RIGHT) dx =  g.speed*3.0f;
            if(d==UP)    dy =  g.speed*3.0f;
            if(d==DOWN)  dy = -g.speed*3.0f;

            float newx = px + dx;
            float newy = py + dy;
            float dist = (newx - pacX)*(newx - pacX) + (newy - pacY)*(newy - pacY);
            if(dist < bestDist) {
                bestDist = dist;
                best = d;
            }
        }
        return best;
    }
    else {
        // Roam: Prefer continuing in current direction, otherwise random
        for(int i=0; i<count; i++) {
            if(possible[i] == g.dir) return g.dir;   // Keep going straight if possible
        }
        return possible[rand() % count];   // Otherwise random valid direction
    }
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

void drawPacman(){
    glColor3f(1,1,0);
    float s,e;
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

void update(int value){
    if(state==PLAYING){
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
                state = MENU;
                cout << "Game Over! Ghost caught Pacman!\n";
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void display(){
    glClear(GL_COLOR_BUFFER_BIT);
    if(state==MENU){
        glColor3f(1,1,1);
        glRasterPos2f(300,350);
        const char* m="Press S to Start";
        for(int i=0;m[i];i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18,m[i]);
    }else{
        drawMaze();
        drawPacman();
        for(int i=0; i<4; i++) drawGhost(ghosts[i]);
    }
    glutSwapBuffers();
}

void keyDown(unsigned char k,int,int){
    k=tolower(k);
    keys[k]=true;
    if(k=='s') state=PLAYING;
}
void keyUp(unsigned char k,int,int){ k=tolower(k); keys[k]=false; }
void specialDown(int k,int,int){specialKeys[k]=true;}
void specialUp(int k,int,int){specialKeys[k]=false;}

int main(int argc,char** argv){
    glutInit(&argc,argv);
    glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGB);
    glutInitWindowSize(W,H);
    glutCreateWindow("Pacman - Ghosts Roam Everywhere");
    gluOrtho2D(0,W,0,H);

    glutDisplayFunc(display);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialDown);
    glutSpecialUpFunc(specialUp);
    glutTimerFunc(0,update,0);

    srand(time(0));

    pacman = Entity(1.5f*CELL, (ROWS-1.5f)*CELL, RIGHT, 3.0f);

    // Ghosts start SCATTERED in different areas of the maze
    ghosts[0] = Ghost(3.5f*CELL,  15.5f*CELL, 1.0f, 0.0f, 0.0f);   // Red - Top leftish
    ghosts[1] = Ghost(15.5f*CELL, 15.5f*CELL, 1.0f, 0.6f, 0.6f);   // Pink - Top right
    ghosts[2] = Ghost(5.5f*CELL,   5.5f*CELL, 0.0f, 1.0f, 1.0f);   // Cyan - Bottom left
    ghosts[3] = Ghost(13.5f*CELL,  8.5f*CELL, 1.0f, 0.6f, 0.0f);   // Orange - Middle right

    glutMainLoop();
    return 0;
}
