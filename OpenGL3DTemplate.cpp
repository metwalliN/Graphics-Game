#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glut.h>

#define GLUT_KEY_ESCAPE 27
#define DEG2RAD(a) (a * 0.0174532925f)

// =========================
// Basic math & Camera (from Lab 6, extended)
// =========================

class Vector3f {
public:
    float x, y, z;

    Vector3f(float _x = 0.0f, float _y = 0.0f, float _z = 0.0f) {
        x = _x;
        y = _y;
        z = _z;
    }

    Vector3f operator+(Vector3f& v) {
        return Vector3f(x + v.x, y + v.y, z + v.z);
    }

    Vector3f operator-(Vector3f& v) {
        return Vector3f(x - v.x, y - v.y, z - v.z);
    }

    Vector3f operator*(float n) {
        return Vector3f(x * n, y * n, z * n);
    }

    Vector3f operator/(float n) {
        return Vector3f(x / n, y / n, z / n);
    }

    Vector3f unit() {
        float len = sqrtf(x * x + y * y + z * z);
        if (len == 0.0f) return Vector3f(0, 0, 0);
        return *this / len;
    }

    Vector3f cross(Vector3f v) {
        return Vector3f(
            y * v.z - z * v.y,
            z * v.x - x * v.z,
            x * v.y - y * v.x
        );
    }
};

class Camera {
public:
    Vector3f eye, center, up;

    Camera(float eyeX = 1.0f, float eyeY = 1.0f, float eyeZ = 1.0f,
        float centerX = 0.0f, float centerY = 0.0f, float centerZ = 0.0f,
        float upX = 0.0f, float upY = 1.0f, float upZ = 0.0f) {
        eye = Vector3f(eyeX, eyeY, eyeZ);
        center = Vector3f(centerX, centerY, centerZ);
        up = Vector3f(upX, upY, upZ);
    }

    void moveX(float d) {
        Vector3f right = up.cross(center - eye).unit();
        eye = eye + right * d;
        center = center + right * d;
    }

    void moveY(float d) {
        Vector3f u = up.unit();
        eye = eye + u * d;
        center = center + u * d;
    }

    void moveZ(float d) {
        Vector3f view = (center - eye).unit();
        eye = eye + view * d;
        center = center + view * d;
    }

    void rotateX(float a) {
        Vector3f view = (center - eye).unit();
        Vector3f right = up.cross(view).unit();
        view = view * cosf(DEG2RAD(a)) + up * sinf(DEG2RAD(a));
        up = view.cross(right);
        center = eye + view;
    }

    void rotateY(float a) {
        Vector3f view = (center - eye).unit();
        Vector3f right = up.cross(view).unit();
        view = view * cosf(DEG2RAD(a)) + right * sinf(DEG2RAD(a));
        right = view.cross(up);
        center = eye + view;
    }

    void look() {
        gluLookAt(
            eye.x, eye.y, eye.z,
            center.x, center.y, center.z,
            up.x, up.y, up.z
        );
    }
};

Camera camera;

// =========================
// Game state definitions
// =========================

enum GameState {
    GAME_PLAYING,
    GAME_WIN,
    GAME_LOSE
};

GameState gameState = GAME_PLAYING;

// World bounds (underwater base perimeter)
const float WORLD_HALF_SIZE = 5.0f;   // walls at ±5 on x and z
const float GROUND_Y = 0.0f;   // seafloor
const float MAX_HEIGHT = 3.0f;   // max swim height above seafloor

// Oxygen timer
float oxygenTime = 60.0f; // 60 seconds of O2
int   lastTimeMs = 0;

// Wall color animation (pulsing underwater lights)
float wallColorPhase = 0.0f;

// =========================
// Player (Diver)
// =========================

struct Player {
    Vector3f pos;
    float radius;      // collision radius
    float rotY;        // yaw: face direction of movement
    float rotX;        // tilt forward when swimming
    bool  onGround;
};

Player diver;

// =========================
// Goal (Oxygen Core)
// =========================

struct Goal {
    Vector3f pos;
    float radius;
    float spinAngle;
    bool  collected;
};

Goal oxygenCore;

// =========================
// Environment objects (5)
// =========================

struct EnvObject {
    Vector3f pos;
    float    animParam;    // angle/offset
    bool     animRunning;
    int      type;         // which model
};

const int NUM_ENV_OBJECTS = 5;
EnvObject envObjects[NUM_ENV_OBJECTS];

// =========================
// Utilities
// =========================

float clampf(float v, float minv, float maxv) {
    if (v < minv) return minv;
    if (v > maxv) return maxv;
    return v;
}

float distSquared(const Vector3f& a, const Vector3f& b) {
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

// =========================
// Drawing helpers
// =========================

void setupLights() {
    // Soft bluish ambient light to feel underwater
    GLfloat ambient[] = { 0.1f, 0.15f, 0.25f, 1.0f };
    GLfloat diffuse[] = { 0.4f, 0.5f, 0.8f, 1.0f };
    GLfloat specular[] = { 0.8f, 0.9f, 1.0f, 1.0f };
    GLfloat shininess[] = { 50.0f };

    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SHININESS, shininess);

    GLfloat lightIntensity[] = { 0.6f, 0.7f, 1.0f, 1.0f };
    GLfloat lightPosition[] = { 0.0f, 5.0f, 0.0f, 1.0f }; // overhead
    glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightIntensity);
}

void setupCamera() {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(60.0, 640.0 / 480.0, 0.1, 100.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    camera.look();
}

// Seafloor
void drawFloor() {
    glPushMatrix();
    glColor3f(0.1f, 0.2f, 0.25f); // dark sand/rocky floor
    glTranslatef(0.0f, GROUND_Y - 0.01f, 0.0f);
    glScalef(WORLD_HALF_SIZE * 2.0f, 0.02f, WORLD_HALF_SIZE * 2.0f);
    glutSolidCube(1.0);
    glPopMatrix();
}

// Boundary walls with animated lights (glowing perimeter of base)
void drawWalls() {
    float r = 0.2f + 0.2f * sinf(wallColorPhase);
    float g = 0.4f + 0.3f * sinf(wallColorPhase + 2.0f);
    float b = 0.7f + 0.3f * sinf(wallColorPhase + 4.0f);

    glColor3f(r, g, b);

    float thickness = 0.2f;
    float height = 2.5f;

    // +Z wall
    glPushMatrix();
    glTranslatef(0.0f, height / 2.0f, WORLD_HALF_SIZE);
    glScalef(WORLD_HALF_SIZE * 2.0f, height, thickness);
    glutSolidCube(1.0);
    glPopMatrix();

    // -Z wall
    glPushMatrix();
    glTranslatef(0.0f, height / 2.0f, -WORLD_HALF_SIZE);
    glScalef(WORLD_HALF_SIZE * 2.0f, height, thickness);
    glutSolidCube(1.0);
    glPopMatrix();

    // +X wall
    glPushMatrix();
    glTranslatef(WORLD_HALF_SIZE, height / 2.0f, 0.0f);
    glScalef(thickness, height, WORLD_HALF_SIZE * 2.0f);
    glutSolidCube(1.0);
    glPopMatrix();

    // -X wall
    glPushMatrix();
    glTranslatef(-WORLD_HALF_SIZE, height / 2.0f, 0.0f);
    glScalef(thickness, height, WORLD_HALF_SIZE * 2.0f);
    glutSolidCube(1.0);
    glPopMatrix();
}

// Diver model: ≥6 primitives
void drawDiver() {
    glPushMatrix();
    glTranslatef(diver.pos.x, diver.pos.y, diver.pos.z);
    glRotatef(diver.rotY, 0, 1, 0);
    glRotatef(diver.rotX, 1, 0, 0);

    // Suit torso
    glPushMatrix();
    glColor3f(0.15f, 0.4f, 0.8f);
    glTranslatef(0.0f, 0.5f, 0.0f);
    glScalef(0.4f, 0.6f, 0.25f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Helmet (head)
    glPushMatrix();
    glColor3f(0.8f, 0.9f, 1.0f);
    glTranslatef(0.0f, 0.95f, 0.05f);
    glutSolidSphere(0.18, 20, 20);
    glPopMatrix();

    // Left arm
    glPushMatrix();
    glColor3f(0.15f, 0.4f, 0.8f);
    glTranslatef(-0.3f, 0.5f, 0.0f);
    glScalef(0.15f, 0.5f, 0.15f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Right arm
    glPushMatrix();
    glColor3f(0.15f, 0.4f, 0.8f);
    glTranslatef(0.3f, 0.5f, 0.0f);
    glScalef(0.15f, 0.5f, 0.15f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Left leg
    glPushMatrix();
    glColor3f(0.05f, 0.2f, 0.5f);
    glTranslatef(-0.12f, 0.15f, 0.0f);
    glScalef(0.15f, 0.5f, 0.15f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Right leg
    glPushMatrix();
    glColor3f(0.05f, 0.2f, 0.5f);
    glTranslatef(0.12f, 0.15f, 0.0f);
    glScalef(0.15f, 0.5f, 0.15f);
    glutSolidCube(1.0);
    glPopMatrix();

    glPopMatrix();
}

// Oxygen core goal: ≥3 primitives, continuous animation
void drawOxygenCore() {
    if (oxygenCore.collected) return;

    glPushMatrix();
    glTranslatef(oxygenCore.pos.x, oxygenCore.pos.y, oxygenCore.pos.z);
    glRotatef(oxygenCore.spinAngle, 0, 1, 0);

    // Glowing center
    glPushMatrix();
    glColor3f(0.1f, 1.0f, 0.9f);
    glutSolidSphere(0.25, 20, 20);
    glPopMatrix();

    // Ring 1
    glPushMatrix();
    glColor3f(0.2f, 0.8f, 0.9f);
    glRotatef(90, 1, 0, 0);
    glutSolidTorus(0.02, 0.35, 20, 20);
    glPopMatrix();

    // Ring 2
    glPushMatrix();
    glColor3f(0.2f, 0.8f, 0.9f);
    glRotatef(90, 0, 0, 1);
    glutSolidTorus(0.02, 0.35, 20, 20);
    glPopMatrix();

    glPopMatrix();
}

// Major object A: Floodlight tower (≥5 primitives)
void drawFloodlightTower() {
    // Base platform
    glPushMatrix();
    glColor3f(0.2f, 0.6f, 0.7f);
    glScalef(0.7f, 0.1f, 0.7f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Vertical pole
    glPushMatrix();
    glColor3f(0.15f, 0.4f, 0.5f);
    glTranslatef(0.0f, 0.7f, 0.0f);
    glScalef(0.15f, 1.4f, 0.15f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Light arm
    glPushMatrix();
    glColor3f(0.3f, 0.7f, 0.9f);
    glTranslatef(0.0f, 1.2f, 0.2f);
    glScalef(0.8f, 0.1f, 0.15f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Light head 1
    glPushMatrix();
    glColor3f(0.9f, 0.95f, 1.0f);
    glTranslatef(-0.25f, 1.2f, 0.35f);
    glutSolidSphere(0.09, 16, 16);
    glPopMatrix();

    // Light head 2
    glPushMatrix();
    glColor3f(0.9f, 0.95f, 1.0f);
    glTranslatef(0.25f, 1.2f, 0.35f);
    glutSolidSphere(0.09, 16, 16);
    glPopMatrix();
}

// Major object B: Sonar / comms array (≥5 primitives)
void drawSonarArray() {
    // Mast
    glPushMatrix();
    glColor3f(0.4f, 0.4f, 0.5f);
    glTranslatef(0.0f, 0.6f, 0.0f);
    glScalef(0.15f, 1.2f, 0.15f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Horizontal boom
    glPushMatrix();
    glColor3f(0.2f, 0.3f, 0.4f);
    glTranslatef(0.0f, 1.1f, 0.0f);
    glScalef(1.4f, 0.08f, 0.15f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Dish 1
    glPushMatrix();
    glColor3f(0.1f, 0.5f, 0.8f);
    glTranslatef(-0.55f, 1.1f, 0.0f);
    glScalef(0.6f, 0.2f, 0.4f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Dish 2
    glPushMatrix();
    glColor3f(0.1f, 0.5f, 0.8f);
    glTranslatef(0.55f, 1.1f, 0.0f);
    glScalef(0.6f, 0.2f, 0.4f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Control module
    glPushMatrix();
    glColor3f(0.5f, 0.6f, 0.7f);
    glTranslatef(0.0f, 0.3f, 0.0f);
    glScalef(0.5f, 0.25f, 0.5f);
    glutSolidCube(1.0);
    glPopMatrix();
}

// Regular object A: Supply crate cluster (≥3 primitives)
void drawSupplyCrates() {
    // Main crate
    glPushMatrix();
    glColor3f(0.45f, 0.3f, 0.2f);
    glScalef(0.5f, 0.4f, 0.5f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Crate 2
    glPushMatrix();
    glColor3f(0.6f, 0.45f, 0.3f);
    glTranslatef(0.4f, 0.2f, 0.2f);
    glScalef(0.3f, 0.3f, 0.3f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Crate 3
    glPushMatrix();
    glColor3f(0.6f, 0.45f, 0.3f);
    glTranslatef(-0.4f, 0.2f, -0.2f);
    glScalef(0.3f, 0.3f, 0.3f);
    glutSolidCube(1.0);
    glPopMatrix();
}

// Regular object B: Repair drone (≥3 primitives)
void drawRepairDrone() {
    // Body
    glPushMatrix();
    glColor3f(0.7f, 0.7f, 0.9f);
    glScalef(0.4f, 0.15f, 0.4f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Sensor eye
    glPushMatrix();
    glColor3f(0.1f, 0.9f, 0.9f);
    glTranslatef(0.0f, 0.0f, 0.25f);
    glutSolidSphere(0.07, 16, 16);
    glPopMatrix();

    // Rotor
    glPushMatrix();
    glColor3f(0.4f, 0.4f, 0.4f);
    glTranslatef(0.2f, 0.1f, 0.2f);
    glScalef(0.2f, 0.02f, 0.2f);
    glutSolidCube(1.0);
    glPopMatrix();
}

// Regular object C: Oxygen tank cluster (≥3 primitives)
void drawOxygenTanks() {
    // Base
    glPushMatrix();
    glColor3f(0.2f, 0.2f, 0.25f);
    glScalef(0.7f, 0.05f, 0.7f);
    glutSolidCube(1.0);
    glPopMatrix();

    // Three tanks
    GLUquadric* quad = gluNewQuadric();

    glPushMatrix();
    glColor3f(0.1f, 0.6f, 0.3f);
    glTranslatef(-0.2f, 0.3f, 0.0f);
    gluCylinder(quad, 0.12, 0.12, 0.8, 20, 20);
    glTranslatef(0.0f, 0.0f, 0.8f);
    glutSolidSphere(0.12, 16, 16);
    glPopMatrix();

    glPushMatrix();
    glColor3f(0.1f, 0.7f, 0.4f);
    glTranslatef(0.0f, 0.3f, 0.0f);
    gluCylinder(quad, 0.12, 0.12, 0.8, 20, 20);
    glTranslatef(0.0f, 0.0f, 0.8f);
    glutSolidSphere(0.12, 16, 16);
    glPopMatrix();

    glPushMatrix();
    glColor3f(0.1f, 0.6f, 0.3f);
    glTranslatef(0.2f, 0.3f, 0.0f);
    gluCylinder(quad, 0.12, 0.12, 0.8, 20, 20);
    glTranslatef(0.0f, 0.0f, 0.8f);
    glutSolidSphere(0.12, 16, 16);
    glPopMatrix();

    gluDeleteQuadric(quad);
}

// Draw environment object by type
void drawEnvObject(const EnvObject& obj) {
    glPushMatrix();
    glTranslatef(obj.pos.x, obj.pos.y, obj.pos.z);

    if (obj.type == 0) {
        // Floodlight tower: rotate slowly in Y to scan
        glRotatef(obj.animParam, 0, 1, 0);
        drawFloodlightTower();
    }
    else if (obj.type == 1) {
        // Sonar array: rotating comms
        glRotatef(obj.animParam, 0, 1, 0);
        drawSonarArray();
    }
    else if (obj.type == 2) {
        // Supply crates: gentle bobbing
        glTranslatef(0.0f, 0.08f * sinf(obj.animParam), 0.0f);
        drawSupplyCrates();
    }
    else if (obj.type == 3) {
        // Repair drone: rotating drone
        glTranslatef(0.0f, 0.4f, 0.0f);
        glRotatef(obj.animParam, 0, 1, 0);
        drawRepairDrone();
    }
    else if (obj.type == 4) {
        // Oxygen tanks: small bob + rotation
        glTranslatef(0.0f, 0.05f * sinf(obj.animParam), 0.0f);
        glRotatef(obj.animParam * 0.5f, 0, 1, 0);
        drawOxygenTanks();
    }

    glPopMatrix();
}

// =========================
// Text rendering (HUD)
// =========================

void drawBitmapText(const char* text, float x, float y) {
    glRasterPos2f(x, y);
    for (size_t i = 0; i < strlen(text); i++) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, text[i]);
    }
}

void drawHUD() {
    // Switch to 2D orthographic for text
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, 1, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 1.0f, 1.0f);

    char buffer[64];
    sprintf(buffer, "O2 Left: %.1f", (oxygenTime > 0.0f ? oxygenTime : 0.0f));
    drawBitmapText(buffer, 0.02f, 0.95f);

    glEnable(GL_LIGHTING);

    // Restore matrices
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// =========================
// Game logic
// =========================

void clampDiverToWorld() {
    diver.pos.x = clampf(diver.pos.x, -WORLD_HALF_SIZE + 0.3f, WORLD_HALF_SIZE - 0.3f);
    diver.pos.z = clampf(diver.pos.z, -WORLD_HALF_SIZE + 0.3f, WORLD_HALF_SIZE - 0.3f);
    diver.pos.y = clampf(diver.pos.y, GROUND_Y, MAX_HEIGHT);

    diver.onGround = (fabs(diver.pos.y - GROUND_Y) < 0.001f);
    if (diver.onGround) {
        diver.rotX = 0.0f;      // standing upright on floor
    }
    else {
        diver.rotX = 25.0f;     // fixed forward tilt when swimming
    }
}

void checkGoalCollision() {
    if (oxygenCore.collected) return;
    float rSum = diver.radius + oxygenCore.radius;
    if (distSquared(diver.pos, oxygenCore.pos) <= rSum * rSum) {
        oxygenCore.collected = true;
        if (gameState == GAME_PLAYING && oxygenTime > 0.0f) {
            gameState = GAME_WIN;
        }
    }
}

// =========================
// Rendering the end screens
// =========================

void drawEndScreen(const char* msg) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, 1, 0, 1);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 1.0f, 1.0f);
    drawBitmapText(msg, 0.4f, 0.5f);
    glEnable(GL_LIGHTING);

    glFlush();
}

// =========================
// GLUT callbacks
// =========================

void Display() {
    if (gameState == GAME_WIN) {
        drawEndScreen("GAME WIN");
        return;
    }
    if (gameState == GAME_LOSE) {
        drawEndScreen("GAME LOSE");
        return;
    }

    setupCamera();
    setupLights();

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawFloor();
    drawWalls();

    // Environment objects
    for (int i = 0; i < NUM_ENV_OBJECTS; ++i) {
        drawEnvObject(envObjects[i]);
    }

    // Oxygen core (goal)
    drawOxygenCore();

    // Diver (player)
    drawDiver();

    // HUD (oxygen timer)
    drawHUD();

    glFlush();
}

// Camera controls from original lab solution
void CameraKeyboard(unsigned char key) {
    float d = 0.1f;

    switch (key) {
    case 'w':
        camera.moveY(d);
        break;
    case 's':
        camera.moveY(-d);
        break;
    case 'a':
        camera.moveX(d);
        break;
    case 'd':
        camera.moveX(-d);
        break;
    case 'q':
        camera.moveZ(d);
        break;
    case 'e':
        camera.moveZ(-d);
        break;
    default:
        break;
    }
}

void Keyboard(unsigned char key, int x, int y) {
    // Escape
    if (key == GLUT_KEY_ESCAPE) {
        exit(EXIT_SUCCESS);
    }

    // Camera movement keys
    if (key == 'w' || key == 's' || key == 'a' || key == 'd' || key == 'q' || key == 'e') {
        CameraKeyboard(key);
        glutPostRedisplay();
        return;
    }

    // Game controls only when playing
    if (gameState == GAME_PLAYING) {
        float step = 0.2f;
        bool moved = false;

        // Diver movement (movement keys also define facing)
        if (key == 'i') { // swim forward (+z)
            diver.pos.z += step;
            diver.rotY = 0.0f;
            moved = true;
        }
        else if (key == 'k') { // swim backward (-z)
            diver.pos.z -= step;
            diver.rotY = 180.0f;
            moved = true;
        }
        else if (key == 'j') { // left (-x)
            diver.pos.x -= step;
            diver.rotY = 90.0f;
            moved = true;
        }
        else if (key == 'l') { // right (+x)
            diver.pos.x += step;
            diver.rotY = -90.0f;
            moved = true;
        }
        else if (key == 'u') { // up (+y)
            diver.pos.y += step;
            moved = true;
        }
        else if (key == 'o') { // down (-y)
            diver.pos.y -= step;
            moved = true;
        }

        // Toggle environment animations (z, x, c, v, b)
        if (key == 'z') {
            envObjects[0].animRunning = !envObjects[0].animRunning;
        }
        else if (key == 'x') {
            envObjects[1].animRunning = !envObjects[1].animRunning;
        }
        else if (key == 'c') {
            envObjects[2].animRunning = !envObjects[2].animRunning;
        }
        else if (key == 'v') {
            envObjects[3].animRunning = !envObjects[3].animRunning;
        }
        else if (key == 'b') {
            envObjects[4].animRunning = !envObjects[4].animRunning;
        }

        // Camera preset views (security cams)
        if (key == '1') { // front view
            camera.eye = Vector3f(0.0f, 3.0f, 10.0f);
            camera.center = Vector3f(0.0f, 0.5f, 0.0f);
            camera.up = Vector3f(0.0f, 1.0f, 0.0f);
        }
        else if (key == '2') { // side view
            camera.eye = Vector3f(10.0f, 3.0f, 0.0f);
            camera.center = Vector3f(0.0f, 0.5f, 0.0f);
            camera.up = Vector3f(0.0f, 1.0f, 0.0f);
        }
        else if (key == '3') { // top sonar view
            camera.eye = Vector3f(0.0f, 15.0f, 0.01f);
            camera.center = Vector3f(0.0f, 0.0f, 0.0f);
            camera.up = Vector3f(0.0f, 0.0f, -1.0f);
        }

        if (moved) {
            clampDiverToWorld();
            checkGoalCollision();
        }
    }

    glutPostRedisplay();
}

void Special(int key, int x, int y) {
    float a = 2.0f;

    switch (key) {
    case GLUT_KEY_UP:
        camera.rotateX(a);
        break;
    case GLUT_KEY_DOWN:
        camera.rotateX(-a);
        break;
    case GLUT_KEY_LEFT:
        camera.rotateY(a);
        break;
    case GLUT_KEY_RIGHT:
        camera.rotateY(-a);
        break;
    }

    glutPostRedisplay();
}

// Idle update: oxygen, animations
void Update() {
    int currentTimeMs = glutGet(GLUT_ELAPSED_TIME);
    float dt = (currentTimeMs - lastTimeMs) / 1000.0f;
    if (dt < 0.0f) dt = 0.0f;
    lastTimeMs = currentTimeMs;

    if (gameState == GAME_PLAYING) {
        // Oxygen timer
        oxygenTime -= dt;
        if (oxygenTime <= 0.0f && !oxygenCore.collected) {
            oxygenTime = 0.0f;
            gameState = GAME_LOSE;
        }

        // Oxygen core animation
        oxygenCore.spinAngle += 60.0f * dt;
        if (oxygenCore.spinAngle > 360.0f) oxygenCore.spinAngle -= 360.0f;

        // Wall light phase
        wallColorPhase += 1.5f * dt;

        // Animate environment objects
        for (int i = 0; i < NUM_ENV_OBJECTS; ++i) {
            if (envObjects[i].animRunning) {
                envObjects[i].animParam += 60.0f * dt;
            }
        }
    }

    glutPostRedisplay();
}

// =========================
// Initialization
// =========================

void initGame() {
    // Diver start position
    diver.pos = Vector3f(0.0f, GROUND_Y, 0.0f);
    diver.radius = 0.4f;
    diver.rotY = 0.0f;
    diver.rotX = 0.0f;
    diver.onGround = true;

    // Oxygen core position
    oxygenCore.pos = Vector3f(2.0f, 0.6f, 2.0f);
    oxygenCore.radius = 0.5f;
    oxygenCore.spinAngle = 0.0f;
    oxygenCore.collected = false;

    // Environment objects placement & types
    // 0: Floodlight tower (major)
    envObjects[0].pos = Vector3f(-3.0f, 0.0f, -2.0f);
    envObjects[0].type = 0;
    envObjects[0].animParam = 0.0f;
    envObjects[0].animRunning = false;

    // 1: Sonar array (major)
    envObjects[1].pos = Vector3f(3.0f, 0.0f, -3.0f);
    envObjects[1].type = 1;
    envObjects[1].animParam = 0.0f;
    envObjects[1].animRunning = false;

    // 2: Supply crates (regular)
    envObjects[2].pos = Vector3f(-2.0f, 0.0f, 3.0f);
    envObjects[2].type = 2;
    envObjects[2].animParam = 0.0f;
    envObjects[2].animRunning = false;

    // 3: Repair drone (regular)
    envObjects[3].pos = Vector3f(2.5f, 0.2f, 0.0f);
    envObjects[3].type = 3;
    envObjects[3].animParam = 0.0f;
    envObjects[3].animRunning = false;

    // 4: Oxygen tanks (regular)
    envObjects[4].pos = Vector3f(0.0f, 0.0f, -3.0f);
    envObjects[4].type = 4;
    envObjects[4].animParam = 0.0f;
    envObjects[4].animRunning = false;

    // Camera default (like external camera looking into base)
    camera.eye = Vector3f(0.0f, 4.0f, 12.0f);
    camera.center = Vector3f(0.0f, 0.5f, 0.0f);
    camera.up = Vector3f(0.0f, 1.0f, 0.0f);

    gameState = GAME_PLAYING;
    oxygenTime = 60.0f;
    lastTimeMs = glutGet(GLUT_ELAPSED_TIME);
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitWindowSize(640, 480);
    glutInitWindowPosition(50, 50);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB | GLUT_DEPTH);

    glutCreateWindow("Underwater Research Base - Oxygen Run");

    glutDisplayFunc(Display);
    glutKeyboardFunc(Keyboard);
    glutSpecialFunc(Special);
    glutIdleFunc(Update);

    glClearColor(0.0f, 0.0f, 0.15f, 0.0f); // deep water blue

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_NORMALIZE);
    glEnable(GL_COLOR_MATERIAL);
    glShadeModel(GL_SMOOTH);

    initGame();

    glutMainLoop();
    return 0;
}
