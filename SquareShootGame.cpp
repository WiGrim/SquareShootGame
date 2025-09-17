#include <GL/glut.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <windows.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdlib>

#define WAV_SHOT 101
#define WAV_MUSIC 102
#define WAV_ZOMBIE1 103
#define WAV_ZOMBIE2 104
#define WAV_ZOMBIE3 105
#define WAV_ZOMBIE4 106
#define WAV_POINTINCREASE 107
#define WAV_LOSING1 108
#define WAV_LOSING2 109


struct WAVData {
    ALenum format;
    ALsizei freq;
    std::vector<char> pcmData;
};

// Загрузка WAV из ресурсов
bool loadWavFromResource(int resId, WAVData& wav) {
    HRSRC hRes = FindResource(nullptr, MAKEINTRESOURCE(resId), L"WAVE");
    if (!hRes) return false;
    HGLOBAL hData = LoadResource(nullptr, hRes);
    if (!hData) return false;
    DWORD size = SizeofResource(nullptr, hRes);
    void* pData = LockResource(hData);
    if (!pData) return false;

    char* ptr = (char*)pData;
    short numChannels = *(short*)(ptr + 22);
    int sampleRate = *(int*)(ptr + 24);
    short bitsPerSample = *(short*)(ptr + 34);
    int dataSize = *(int*)(ptr + 40);

    if (numChannels == 1)
        wav.format = (bitsPerSample == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
    else if (numChannels == 2)
        wav.format = (bitsPerSample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
    else return false;

    wav.freq = sampleRate;
    wav.pcmData.assign(ptr + 44, ptr + 44 + dataSize);
    return true;
}

// Создаём источник с цикличным воспроизведением
ALuint createLoopingSource(ALuint buffer, float volume) {
    ALuint source;
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, buffer);
    alSourcef(source, AL_GAIN, volume);
    alSourcei(source, AL_LOOPING, AL_TRUE);
    alSourcePlay(source);
    return source;
}

struct Bullet {
    float x, y;
    Bullet(float _x, float _y) : x(_x), y(_y) {}
    void update() { y += 0.02f; }
    void draw() {
        glBegin(GL_QUADS);
        glVertex2f(x - 0.01f, y);
        glVertex2f(x + 0.01f, y);
        glVertex2f(x + 0.01f, y + 0.05f);
        glVertex2f(x - 0.01f, y + 0.05f);
        glEnd();
    }
};

struct Enemy {
    float x, y;
    Enemy(float _x, float _y) : x(_x), y(_y) {}
    void update() { y -= 0.005f; }
    void draw() {
        glBegin(GL_QUADS);
        glVertex2f(x - 0.05f, y - 0.05f);
        glVertex2f(x + 0.05f, y - 0.05f);
        glVertex2f(x + 0.05f, y + 0.05f);
        glVertex2f(x - 0.05f, y + 0.05f);
        glEnd();
    }
};

float playerX = 0.0f;
std::vector<Bullet> bullets;
std::vector<Enemy> enemies;

ALuint bufShot, bufMusic;
float volumeMusic = 0.25f;
float volumeShot = 0.25f;
std::vector<ALuint> activeShots;

void drawPlayer() {
    glBegin(GL_QUADS);
    glVertex2f(playerX - 0.05f, -0.9f);
    glVertex2f(playerX + 0.05f, -0.9f);
    glVertex2f(playerX + 0.05f, -0.8f);
    glVertex2f(playerX - 0.05f, -0.8f);
    glEnd();
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    glColor3f(0, 1, 1);
    drawPlayer();

    glColor3f(1, 1, 0);
    for (auto& b : bullets) b.draw();

    glColor3f(1, 0, 0);
    for (auto& e : enemies) e.draw();

    glutSwapBuffers();
}

void update(int value) {
    for (auto& b : bullets) b.update();
    bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
        [](Bullet& b) { return b.y > 1; }),
        bullets.end());

    for (auto& e : enemies) e.update();

    // Столкновения
    for (auto& e : enemies) {
        for (auto& b : bullets) {
            if (fabs(b.x - e.x) < 0.05f && fabs(b.y - e.y) < 0.05f) {
                e.y = -10;
                b.y = 10;
            }
        }
    }

    // Движение игрока
    if (GetAsyncKeyState(VK_LEFT) & 0x8000) if (playerX > -0.95f) playerX -= 0.02f;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) if (playerX < 0.95f) playerX += 0.02f;

    enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
        [](Enemy& e) { return e.y < -1; }),
        enemies.end());

    // Спавн врагов
    if (rand() % 50 == 0) enemies.push_back(Enemy((rand() % 200 - 100) / 100.0f, 1.0f));

    // Очистка завершившихся источников OpenAL
    for (auto it = activeShots.begin(); it != activeShots.end();) {
        ALint state;
        alGetSourcei(*it, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) {
            alDeleteSources(1, &(*it));
            it = activeShots.erase(it);
        }
        else ++it;
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void keyboard(unsigned char key, int x, int y) {
    if (key == 32) { // пробел
        bullets.push_back(Bullet(playerX, -0.8f));

        ALuint src;
        alGenSources(1, &src);
        alSourcei(src, AL_BUFFER, bufShot);
        alSourcef(src, AL_GAIN, volumeShot);
        alSourcePlay(src);
        activeShots.push_back(src);
    }
}

int main(int argc, char** argv) {
    // OpenAL
    ALCdevice* device = alcOpenDevice(nullptr);
    if (!device) { std::cerr << "Не удалось открыть устройство\n"; return -1; }
    ALCcontext* context = alcCreateContext(device, nullptr);
    alcMakeContextCurrent(context);

    WAVData shot, music;
    if (!loadWavFromResource(WAV_SHOT, shot) || !loadWavFromResource(WAV_MUSIC, music)) {
        std::cerr << "Не удалось загрузить WAV ресурсы\n"; return -1;
    }

    alGenBuffers(1, &bufShot);
    alGenBuffers(1, &bufMusic);
    alBufferData(bufShot, shot.format, shot.pcmData.data(), static_cast<ALsizei>(shot.pcmData.size()), shot.freq);
    alBufferData(bufMusic, music.format, music.pcmData.data(), static_cast<ALsizei>(music.pcmData.size()), music.freq);

    ALuint srcMusic = createLoopingSource(bufMusic, volumeMusic);

    // OpenGL
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 600);
    glutCreateWindow("OpenGL Shooter");

    glClearColor(0, 0, 0, 1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1, 1, -1, 1);

    glutDisplayFunc(display);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, update, 0);

    glutMainLoop();

    // Очистка OpenAL
    for (ALuint src : activeShots) alDeleteSources(1, &src);
    alDeleteSources(1, &srcMusic);
    alDeleteBuffers(1, &bufShot);
    alDeleteBuffers(1, &bufMusic);
    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context);
    alcCloseDevice(device);

    return 0;
}
