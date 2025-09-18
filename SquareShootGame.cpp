#include <GL/glut.h>
#include <AL/al.h>
#include <AL/alc.h>
#include <windows.h>
#include <vector>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <string>
#include <sstream>
#include <conio.h>

#define WAV_SHOT 101
#define WAV_MUSIC 102
#define WAV_ZOMBIE1 103
#define WAV_ZOMBIE2 104
#define WAV_ZOMBIE3 105
#define WAV_ZOMBIE4 106
#define WAV_POINTINCREASE 107
#define WAV_LOSING1 108
#define WAV_LOSING2 109

// Координаты игрока
float playerX = 0.0f;
float playerY = -0.8f;
// Направление стрельбы
float shootDirX = 0.0f;
float shootDirY = 1.0f;

float bulletSpeed = 0.035f; // Скорость пуль
float playerSpeed = 0.017f; // Скорость игрока
float baseZombieSpeed = 0.01f; // Скорость зомби
float maximumZombieSpeedChange = 0.004f; // Величина возможного отклонения скорости зомби

float playerSize = 0.08f; // Размер игрока
float indicatorSize = 0.015f; // Размер индикатора направления
float bulletSize = 0.02f; // Размер пули
float enemySize = 0.1f; // Размер противников

// Служебные переменные
int shotCooldown = 0;

int zombieSoundTimer = 0;
int lastZombieChoice = -1;

int score = 0;
float endTime = -1;
float startTime = float(clock());
bool gameOver = false;
float gameOverTextTime = 0.0f;

std::vector<ALuint> losingQueue;
ALuint currentLosingSource = 0;
ALuint srcMusic;

// Настройки громкости звуков
float volumeMusic = 0.18f;
float volumeShot = 0.18f;
/*float volumeZombie1 = 0.25f;
float volumeZombie2 = 0.25f;
float volumeZombie3 = 0.25f;
float volumeZombie4 = 0.25f;*/
float volumePointIncrease = 0.25f;
float volumeLosing1 = 0.25f;
float volumeLosing2 = 0.25f;

// Бессмертие
bool godMode = false;

struct WAVData {
    ALenum format;
    ALsizei freq;
    std::vector<char> pcmData;
};

struct Bullet {
    float x, y;
    float dx, dy;

    Bullet(float _x, float _y, float _dx, float _dy) : x(_x), y(_y), dx(_dx), dy(_dy) {}

    void update() {
        x += dx;
        y += dy;
    }

    void draw() {
        glColor3f(1, 1, 0);
        glBegin(GL_QUADS);
        glVertex2f(x - bulletSize / 2, y - bulletSize / 2);
        glVertex2f(x + bulletSize / 2, y - bulletSize / 2);
        glVertex2f(x + bulletSize / 2, y + bulletSize / 2);
        glVertex2f(x - bulletSize / 2, y + bulletSize / 2);
        glEnd();
    }
};

float randomSpeed(float baseSpeed, float variation) {
    float r = (rand() / (float)RAND_MAX);
    return baseSpeed - variation + r * (2 * variation);
}

struct Enemy {
    float x, y;
    float dx, dy;
    bool alive = true;

    Enemy(float _x, float _y, float targetX, float targetY) : x(_x), y(_y) {
        float dirX = targetX - _x;
        float dirY = targetY - _y;
        float len = sqrt(dirX * dirX + dirY * dirY);

        float speed = randomSpeed(baseZombieSpeed, maximumZombieSpeedChange);
        dx = (len > 0) ? dirX / len * speed : 0;
        dy = (len > 0) ? dirY / len * speed : 0;
    }

    void update() {
        x += dx;
        y += dy;
    }

    void draw() {
        glColor3f(1, 0, 0);
        glBegin(GL_QUADS);
        glVertex2f(x - enemySize / 2, y - enemySize / 2);
        glVertex2f(x + enemySize / 2, y - enemySize / 2);
        glVertex2f(x + enemySize / 2, y + enemySize / 2);
        glVertex2f(x - enemySize / 2, y + enemySize / 2);
        glEnd();
    }
};

struct Record {
    std::string name;
    int score;
    float time;
};

void saveRecord(const std::string& playerName, int score, float time) {
    std::ifstream infile("Records.txt");
    std::ofstream outfile("Records_tmp.txt");
    bool inserted = false;
    std::string line;
    while (std::getline(infile, line)) {
        std::stringstream ss(line);
        std::string name, tmp;
        int existingScore;
        float existingTime;
        ss >> name >> tmp >> existingScore >> tmp >> existingTime;
        if (!inserted) {
            if (score > existingScore || (score == existingScore && time > existingTime)) {
                outfile << playerName << " Score: " << score << " Time: " << time << "\n";
                inserted = true;
            }
        }
        outfile << line << "\n";
    }
    if (!inserted) {
        outfile << playerName << " Score: " << score << " Time: " << time << "\n";
    }
    infile.close();
    outfile.close();
    std::remove("Records.txt");
    std::rename("Records_tmp.txt", "Records.txt");
}

std::vector<Bullet> bullets;
std::vector<Enemy> enemies;
std::vector<ALuint> activeShots;
std::vector<ALuint> activeZombieSounds;
ALuint bufShot, bufMusic, bufZombie1, bufZombie2, bufZombie3, bufZombie4, bufPointIncrease, bufLosing1, bufLosing2;

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

// Загрузка буферов
bool loadBufferFromResource(int resId, ALuint& buffer) {
    WAVData wav;
    if (!loadWavFromResource(resId, wav)) {
        std::cerr << "Ошибка загрузки ресурса " << resId << "\n";
        return false;
    }
    alGenBuffers(1, &buffer);
    alBufferData(buffer, wav.format, wav.pcmData.data(),
        static_cast<ALsizei>(wav.pcmData.size()), wav.freq);
    return true;
}

// Источник с цикличным воспроизведением
ALuint createLoopingSource(ALuint buffer, float volume) {
    ALuint source;
    alGenSources(1, &source);
    alSourcei(source, AL_BUFFER, buffer);
    alSourcef(source, AL_GAIN, volume);
    alSourcei(source, AL_LOOPING, AL_TRUE);
    alSourcePlay(source);
    return source;
}

void drawPlayer() {
    // Игрок
    glColor3f(0, 0.9, 0.9);
    glBegin(GL_QUADS);
    glVertex2f(playerX - playerSize / 2, playerY - playerSize / 2);
    glVertex2f(playerX + playerSize / 2, playerY - playerSize / 2);
    glVertex2f(playerX + playerSize / 2, playerY + playerSize / 2);
    glVertex2f(playerX - playerSize / 2, playerY + playerSize / 2);
    glEnd();

    // Индикатор направления
    glColor3f(0.7, 0.3, 0);
    glBegin(GL_QUADS);
    glVertex2f(playerX + shootDirX * (playerSize / 2 + 0.013) - indicatorSize, playerY + shootDirY * (playerSize / 2 + 0.013) - indicatorSize);
    glVertex2f(playerX + shootDirX * (playerSize / 2 + 0.013) + indicatorSize, playerY + shootDirY * (playerSize / 2 + 0.013) - indicatorSize);
    glVertex2f(playerX + shootDirX * (playerSize / 2 + 0.013) + indicatorSize, playerY + shootDirY * (playerSize / 2 + 0.013) + indicatorSize);
    glVertex2f(playerX + shootDirX * (playerSize / 2 + 0.013) - indicatorSize, playerY + shootDirY * (playerSize / 2 + 0.013) + indicatorSize);
    glEnd();
}

// Функция вывода текста
void drawText(float x, float y, const char* text) {
    glColor3f(1, 1, 1);
    glRasterPos2f(x, y);
    while (*text) {
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *text);
        text++;
    }
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    drawPlayer();
    for (auto& b : bullets) b.draw();
    for (auto& e : enemies) e.draw();

    char buffer[64];
    sprintf_s(buffer, sizeof(buffer), "Score: %d", score);
    drawText(-0.95f, 0.9f, buffer);

    if (!gameOver) {
        endTime = (float(clock()) - startTime) / CLOCKS_PER_SEC;
        sprintf_s(buffer, sizeof(buffer), "Time: %.1f s", endTime);
        drawText(-0.95f, 0.8f, buffer);
    }
    else
    {
        sprintf_s(buffer, sizeof(buffer), "Time: %.1f s", endTime);
        drawText(-0.95f, 0.8f, buffer);
        float t = (float)clock() / CLOCKS_PER_SEC - gameOverTextTime;
        if (((int)(t * 2)) % 2 == 0) {
            drawText(-0.2f, 0.0f, "GAME OVER");
        }
    }

    glutSwapBuffers();
}

void startLosingSounds() {
    if (!losingQueue.empty()) {
        currentLosingSource = 0;
        ALuint buffer = losingQueue.front();
        losingQueue.erase(losingQueue.begin());
        alGenSources(1, &currentLosingSource);
        alSourcei(currentLosingSource, AL_BUFFER, buffer);
        alSourcef(currentLosingSource, AL_GAIN, 0.25f);
        alSourcePlay(currentLosingSource);
    }
}

void endGame() {
    endTime = (float(clock()) - startTime) / CLOCKS_PER_SEC;
    alSourceStop(srcMusic);

    losingQueue.clear();
    losingQueue.push_back(bufLosing1);
    losingQueue.push_back(bufLosing2);
    startLosingSounds();

    gameOver = true;
    gameOverTextTime = (float)clock() / CLOCKS_PER_SEC;
}


void update(int value) {
    if (!gameOver) {

        // Обновление пуль
        for (auto& b : bullets) b.update();
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [](Bullet& b) { return b.y > 1; }),
            bullets.end());

        // Обновление врагов
        for (auto& e : enemies) e.update();

        // Стрельба
        bool shooting = false;
        if (GetAsyncKeyState(VK_UP) & 0x8000) { shootDirX = 0; shootDirY = 1; shooting = true; }
        if (GetAsyncKeyState(VK_DOWN) & 0x8000) { shootDirX = 0; shootDirY = -1; shooting = true; }
        if (GetAsyncKeyState(VK_LEFT) & 0x8000) { shootDirX = -1; shootDirY = 0; shooting = true; }
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000) { shootDirX = 1; shootDirY = 0; shooting = true; }

        if (shooting && shotCooldown <= 0) {
            float len = sqrt(shootDirX * shootDirX + shootDirY * shootDirY);
            float dx = (len > 0) ? shootDirX / len * bulletSpeed : 0;
            float dy = (len > 0) ? shootDirY / len * bulletSpeed : bulletSpeed;

            float startX = playerX + shootDirX * 0.06f;
            float startY = playerY + shootDirY * 0.06f;

            bullets.push_back(Bullet(startX, startY, dx, dy));

            ALuint src;
            alGenSources(1, &src);
            alSourcei(src, AL_BUFFER, bufShot);
            alSourcef(src, AL_GAIN, volumeShot);
            alSourcePlay(src);
            activeShots.push_back(src);

            shotCooldown = 250;
        }
        if (shotCooldown > 0) shotCooldown -= 16;

        // Звуки зомби
        if (zombieSoundTimer <= 0) {
            int choice = rand() % 4;
            while (choice == lastZombieChoice) choice = rand() % 4;
            lastZombieChoice = choice;
            ALuint buffer =
                (choice == 0) ? bufZombie1 :
                (choice == 1) ? bufZombie2 :
                (choice == 2) ? bufZombie3 :
                bufZombie4;

            ALuint src;
            alGenSources(1, &src);
            alSourcei(src, AL_BUFFER, buffer);
            alSourcef(src, AL_GAIN, 0.3f);
            alSourcePlay(src);
            activeZombieSounds.push_back(src);

            zombieSoundTimer = 2000 + rand() % 5000;
        }
        else zombieSoundTimer -= 16;

        // Столкновения пули - враг
        for (auto& e : enemies) {
            if (!e.alive) continue;
            for (auto& b : bullets) {
                if (fabs(b.x - e.x) < 0.05f && fabs(b.y - e.y) < 0.05f) {
                    e.alive = false;
                    b.y = 10;
                    score++;
                }
            }
        }

        // Столкновение игрок - враг
        if (!godMode) {
            for (auto& e : enemies) {
                if (!e.alive) continue;
                if (fabs(playerX - e.x) < 0.05f + 0.05f && fabs(playerY - e.y) < 0.05f + 0.05f) {
                    endGame();
                    break;
                }
            }
        }

        // Удаляем мертвых врагов
        enemies.erase(std::remove_if(enemies.begin(), enemies.end(),
            [](Enemy& e) { return !e.alive || e.y < -1; }),
            enemies.end());

        // Движение игрока
        if (GetAsyncKeyState('A') & 0x8000) if (playerX > -0.95f) playerX -= playerSpeed;
        if (GetAsyncKeyState('D') & 0x8000) if (playerX < 0.95f) playerX += playerSpeed;
        if (GetAsyncKeyState('W') & 0x8000) if (playerY < 0.95f) playerY += playerSpeed;
        if (GetAsyncKeyState('S') & 0x8000) if (playerY > -0.95f) playerY -= playerSpeed;

        // Спавн врагов
        float timeElapsed = (float(clock()) - startTime) / CLOCKS_PER_SEC;
        float spawnInterval = 28.0f - timeElapsed * 0.167f;
        if (spawnInterval < 10.0f) spawnInterval = 10.0f;

        if (rand() % (int)spawnInterval == 0) {
            int side = rand() % 4;
            float ex, ey;
            switch (side) {
            case 0: ex = (rand() % 200 - 100) / 100.0f; ey = 1.1f; break;
            case 1: ex = (rand() % 200 - 100) / 100.0f; ey = -1.1f; break;
            case 2: ex = -1.1f; ey = (rand() % 200 - 100) / 100.0f; break;
            case 3: ex = 1.1f; ey = (rand() % 200 - 100) / 100.0f; break;
            }
            enemies.push_back(Enemy(ex, ey, playerX, playerY));
        }

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
    }
    else {
        if (currentLosingSource != 0) {
            ALint state;
            alGetSourcei(currentLosingSource, AL_SOURCE_STATE, &state);
            if (state != AL_PLAYING) {
                alDeleteSources(1, &currentLosingSource);
                currentLosingSource = 0;
                startLosingSounds();
            }
        }
        else
        {
            std::string name;
            std::cout << "Game Over! Enter your name: ";
            std::getline(std::cin, name);
            if (!name.empty()) {
                saveRecord(name, score, endTime);
                std::cout << "Record saved!\n";
            }
            exit(0);
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

void moveConsole(HWND hwndGL) {
    HWND console = GetConsoleWindow();
    if (console && hwndGL) {
        RECT glRect;
        GetWindowRect(hwndGL, &glRect);

        RECT consoleRect;
        GetWindowRect(console, &consoleRect);
        int consoleWidth = consoleRect.right - consoleRect.left;
        int consoleHeight = consoleRect.bottom - consoleRect.top;

        int consoleX = glRect.right + 5;
        int consoleY = glRect.top;

        MoveWindow(console, consoleX, consoleY, consoleWidth, consoleHeight, TRUE);
    }
}

int main(int argc, char** argv) {
    srand(static_cast<unsigned>(time(nullptr)));
    clock_t startTime = clock();
    // OpenAL
    ALCdevice* device = alcOpenDevice(nullptr);
    if (!device) { std::cerr << "Не удалось открыть устройство\n"; return -1; }
    ALCcontext* context = alcCreateContext(device, nullptr);
    alcMakeContextCurrent(context);

    if (!loadBufferFromResource(WAV_SHOT, bufShot) ||
        !loadBufferFromResource(WAV_MUSIC, bufMusic) ||
        !loadBufferFromResource(WAV_ZOMBIE1, bufZombie1) ||
        !loadBufferFromResource(WAV_ZOMBIE2, bufZombie2) ||
        !loadBufferFromResource(WAV_ZOMBIE3, bufZombie3) ||
        !loadBufferFromResource(WAV_ZOMBIE4, bufZombie4) ||
        !loadBufferFromResource(WAV_POINTINCREASE, bufPointIncrease) ||
        !loadBufferFromResource(WAV_LOSING1, bufLosing1) ||
        !loadBufferFromResource(WAV_LOSING2, bufLosing2))
        return -1;

    srcMusic = createLoopingSource(bufMusic, volumeMusic);

    // OpenGL
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(800, 800);
    glutCreateWindow("Square Shoot Game");
    HWND hwndGL = FindWindow(nullptr, L"Square Shoot Game");
    moveConsole(hwndGL);

    glClearColor(0, 0, 0, 1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1, 1, -1, 1);

    glutDisplayFunc(display);
    glutTimerFunc(16, update, 0);

    glutMainLoop();

    // Очистка OpenAL
    for (ALuint src : activeShots) alDeleteSources(1, &src);
    for (ALuint src : activeZombieSounds) alDeleteSources(1, &src);
    alDeleteSources(1, &srcMusic);
    alDeleteBuffers(1, &bufShot);
    alDeleteBuffers(1, &bufMusic);
    alDeleteBuffers(1, &bufZombie1);
    alDeleteBuffers(1, &bufZombie2);
    alDeleteBuffers(1, &bufZombie3);
    alDeleteBuffers(1, &bufZombie4);
    alDeleteBuffers(1, &bufPointIncrease);
    alDeleteBuffers(1, &bufLosing1);
    alDeleteBuffers(1, &bufLosing2);

    alcMakeContextCurrent(nullptr);
    alcDestroyContext(context);
    alcCloseDevice(device);

    return 0;
}
