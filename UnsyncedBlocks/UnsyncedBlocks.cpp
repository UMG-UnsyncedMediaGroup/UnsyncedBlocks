#include <Windows.h>
#include <windowsx.h>
#include <gl/GL.h>
#include <gl/GLU.h>
#include <cmath>
#include <vector>
#include <string>
#include <algorithm>
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#undef min

struct Block { float x, y, z; };
struct Player { float x = 0.0f, y = 1.8f, z = 5.0f; float width = 0.5f; float height = 1.8f; };

Player player;
float camYaw = 0.0f, camPitch = 0.0f;

float vel[3] = { 0, 0, 0 };
bool onGround = true;

bool keyW = false, keyA = false, keyS = false, keyD = false, keySpace = false;
POINT centerPos;
float sensitivity = 0.15f;

std::vector<Block> blocks;

GLfloat lightDirection[4] = { -0.5f, -1.0f, -0.5f, 0.0f };
GLfloat ambientLight[4] = { 0.2f, 0.2f, 0.2f, 1.0f };
GLfloat diffuseLight[4] = { 0.8f, 0.8f, 0.8f, 1.0f };
GLfloat specularLight[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

HDC hdcGlobal;
GLuint fontBase;

bool CheckCollision(const Player& p, const Block& b)
{
    float px1 = p.x - p.width, px2 = p.x + p.width;
    float py1 = p.y, py2 = p.y + p.height;
    float pz1 = p.z - p.width, pz2 = p.z + p.width;

    float bx1 = b.x - 1, bx2 = b.x + 1;
    float by1 = b.y, by2 = b.y + 2;
    float bz1 = b.z - 1, bz2 = b.z + 1;

    return (px2 > bx1 && px1 < bx2) && (py2 > by1 && py1 < by2) && (pz2 > bz1 && pz1 < bz2);
}

void DrawBaseplate()
{
    int size = 50;
    glBegin(GL_QUADS);
    for (int x = -size; x < size; x += 2)
    {
        for (int z = -size; z < size; z += 2)
        {
            float ao = ((x + size) % 4 == 0 && (z + size) % 4 == 0) ? 0.7f : 1.0f;
            glColor3f(0.2f * ao, 0.8f * ao, 0.2f * ao);
            glVertex3f(x, 0, z);
            glVertex3f(x + 2, 0, z);
            glVertex3f(x + 2, 0, z + 2);
            glVertex3f(x, 0, z + 2);
        }
    }
    glEnd();
}

void DrawBlock(const Block& block)
{
    glPushMatrix();
    glTranslatef(block.x, block.y, block.z);

    GLfloat matSpecular[] = { 0.5f,0.5f,0.5f,1.0f };
    GLfloat matShininess[] = { 32.0f };
    glMaterialfv(GL_FRONT, GL_SPECULAR, matSpecular);
    glMaterialfv(GL_FRONT, GL_SHININESS, matShininess);

    float topColor[3] = { 0.8f,0.3f,0.3f };
    float sideColor[3] = { 0.6f,0.2f,0.2f };
    float w = 1.0f, h = 2.0f, d = 1.0f;

    glBegin(GL_QUADS);
    glColor3fv(topColor); glNormal3f(0, 1, 0);
    glVertex3f(-w, h, -d); glVertex3f(w, h, -d); glVertex3f(w, h, d); glVertex3f(-w, h, d);
    glColor3fv(sideColor); glNormal3f(0, -1, 0);
    glVertex3f(-w, 0, -d); glVertex3f(w, 0, -d); glVertex3f(w, 0, d); glVertex3f(-w, 0, d);
    glNormal3f(0, 0, 1); glVertex3f(-w, 0, d); glVertex3f(w, 0, d); glVertex3f(w, h, d); glVertex3f(-w, h, d);
    glNormal3f(0, 0, -1); glVertex3f(-w, 0, -d); glVertex3f(w, 0, -d); glVertex3f(w, h, -d); glVertex3f(-w, h, -d);
    glNormal3f(-1, 0, 0); glVertex3f(-w, 0, -d); glVertex3f(-w, 0, d); glVertex3f(-w, h, d); glVertex3f(-w, h, -d);
    glNormal3f(1, 0, 0); glVertex3f(w, 0, -d); glVertex3f(w, 0, d); glVertex3f(w, h, d); glVertex3f(w, h, -d);
    glEnd();

    glPopMatrix();
}

void DrawSoftShadow(const Block& block, float lightDir[3])
{
    float lx = lightDir[0], ly = lightDir[1], lz = lightDir[2];
    if (ly == 0.0f) ly = 0.001f;

    GLfloat shadowMat[16] = {
        1, 0, 0, 0,
        -lx / ly, 0, -lz / ly, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };

    glPushMatrix();
    glTranslatef(block.x, block.y, block.z);
    glMultMatrixf(shadowMat);

    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float w = 1.0f, d = 1.0f;
    int layers = 3;
    for (int layer = 0; layer < layers; ++layer)
    {
        float ao = 1.0f - std::min(layer * 0.15f, 0.6f);
        float yOffset = layer * 0.02f;
        glColor4f(0.0f, 0.0f, 0.0f, 0.25f * ao);

        glBegin(GL_QUADS);
        glVertex3f(-w, yOffset, -d); glVertex3f(w, yOffset, -d);
        glVertex3f(w, yOffset, d); glVertex3f(-w, yOffset, d);
        glEnd();
    }

    glDisable(GL_BLEND);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void DrawText2D(const std::string& text, float x, float y)
{
    glMatrixMode(GL_PROJECTION); glPushMatrix();
    glLoadIdentity(); gluOrtho2D(0, 800, 0, 600);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();

    glDisable(GL_LIGHTING);
    glColor3f(1.0f, 1.0f, 0.0f);
    glRasterPos2f(x, 600 - y);
    for (char c : text) glCallList(fontBase + c);
    glEnable(GL_LIGHTING);

    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void CreateFont()
{
    HFONT font = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
        ANTIALIASED_QUALITY, FF_DONTCARE, "Arial");
    SelectObject(hdcGlobal, font);

    fontBase = glGenLists(256);
    wglUseFontBitmapsA(hdcGlobal, 0, 256, fontBase);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE: PostQuitMessage(0); return 0;
    case WM_SIZE: glViewport(0, 0, LOWORD(lParam), HIWORD(lParam)); return 0;
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) PostQuitMessage(0);
        if (wParam == 'W') keyW = true;
        if (wParam == 'A') keyA = true;
        if (wParam == 'S') keyS = true;
        if (wParam == 'D') keyD = true;
        if (wParam == VK_SPACE) keySpace = true;
        return 0;
    case WM_KEYUP:
        if (wParam == 'W') keyW = false;
        if (wParam == 'A') keyA = false;
        if (wParam == 'S') keyS = false;
        if (wParam == 'D') keyD = false;
        if (wParam == VK_SPACE) keySpace = false;
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) != WA_INACTIVE)
        {
            RECT rect; GetClientRect(hwnd, &rect);
            centerPos.x = rect.right / 2; centerPos.y = rect.bottom / 2;
            ClientToScreen(hwnd, &centerPos);
            SetCursorPos(centerPos.x, centerPos.y);
            ShowCursor(FALSE);
        }
        else ShowCursor(TRUE);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int)
{
    WNDCLASS wc = {};
    wc.style = CS_OWNDC; wc.lpfnWndProc = WindowProc; wc.hInstance = hInstance;
    wc.lpszClassName = L"UnsyncedBlocks"; RegisterClass(&wc);
    HWND hwnd = CreateWindowEx(0, L"UnsyncedBlocks", L"UnsyncedBlocks",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        nullptr, nullptr, hInstance, nullptr);
    HDC hdc = GetDC(hwnd); hdcGlobal = hdc;
    ShowWindow(hwnd, SW_SHOW);

    RECT rect; GetClientRect(hwnd, &rect);
    centerPos.x = rect.right / 2; centerPos.y = rect.bottom / 2;
    ClientToScreen(hwnd, &centerPos); SetCursorPos(centerPos.x, centerPos.y);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(pfd); pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA; pfd.cColorBits = 32;
    int pf = ChoosePixelFormat(hdc, &pfd); SetPixelFormat(hdc, pf, &pfd);
    HGLRC glrc = wglCreateContext(hdc); wglMakeCurrent(hdc, glrc);

    CreateFont();

    glEnable(GL_DEPTH_TEST); glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL); glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    glShadeModel(GL_SMOOTH);
    glLightfv(GL_LIGHT0, GL_POSITION, lightDirection);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);

    blocks.push_back({ 0,0,0 }); blocks.push_back({ 3,0,0 }); blocks.push_back({ 3,2,0 });
    blocks.push_back({ -3,0,0 }); blocks.push_back({ 0,0,3 }); blocks.push_back({ 0,0,-3 });

    MSG msg{}; float speed = 0.1f;
    DWORD lastTime = GetTickCount(), frameCount = 0;
    float fps = 0.0f;

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); }
        else
        {
            DWORD currentTime = GetTickCount();
            frameCount++;
            if (currentTime - lastTime >= 1000)
            {
                fps = frameCount * 1000.0f / (currentTime - lastTime);
                frameCount = 0;
                lastTime = currentTime;
            }

            if (GetForegroundWindow() == hwnd)
            {
                POINT mousePos; GetCursorPos(&mousePos);
                int dx = mousePos.x - centerPos.x, dy = mousePos.y - centerPos.y;
                camYaw += dx * sensitivity; camPitch -= dy * sensitivity;
                if (camPitch > 89.0f) camPitch = 89.0f;
                if (camPitch < -89.0f) camPitch = -89.0f;
                SetCursorPos(centerPos.x, centerPos.y);

                float radYaw = camYaw * 3.14159f / 180.0f;
                float forward[3] = { sin(radYaw), 0, -cos(radYaw) };
                float right[3] = { cos(radYaw), 0, sin(radYaw) };

                float wishVel[3] = { 0, 0, 0 };
                if (keyW) { wishVel[0] += forward[0]; wishVel[2] += forward[2]; }
                if (keyS) { wishVel[0] -= forward[0]; wishVel[2] -= forward[2]; }
                if (keyA) { wishVel[0] -= right[0]; wishVel[2] -= right[2]; }
                if (keyD) { wishVel[0] += right[0]; wishVel[2] += right[2]; }

                float len = sqrt(wishVel[0] * wishVel[0] + wishVel[2] * wishVel[2]);
                if (len > 0.0f) { wishVel[0] /= len; wishVel[2] /= len; }

                float accel = onGround ? 0.015f : 0.008f;
                vel[0] += wishVel[0] * accel;
                vel[2] += wishVel[2] * accel;

                float speed = sqrt(vel[0] * vel[0] + vel[2] * vel[2]);
                float maxSpeed = 0.2f;
                if (speed > maxSpeed) { vel[0] = vel[0] / speed * maxSpeed; vel[2] = vel[2] / speed * maxSpeed; }

                if (onGround) { vel[0] *= 0.8f; vel[2] *= 0.8f; }

                vel[1] -= 0.01f; 
                if (keySpace && onGround) { vel[1] = 0.18f; onGround = false; }

                player.x += vel[0];
                for (const auto& b : blocks) if (CheckCollision(player, b)) { player.x -= vel[0]; vel[0] = 0; }

                player.y += vel[1];
                onGround = false;

                if (player.y <= 1.8f) { player.y = 1.8f; vel[1] = 0; onGround = true; }
                for (const auto& b : blocks)
                {
                    if (CheckCollision(player, b))
                    {
                        if (vel[1] < 0) { player.y = b.y + 2; vel[1] = 0; onGround = true; }
                        else if (vel[1] > 0) { player.y = b.y - player.height; vel[1] = 0; }
                    }
                }

                player.z += vel[2];
                for (const auto& b : blocks) if (CheckCollision(player, b)) { player.z -= vel[2]; vel[2] = 0; }

                glClearColor(0.4f, 0.6f, 0.9f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

                glMatrixMode(GL_PROJECTION); glLoadIdentity();
                gluPerspective(70.0, 800.0 / 600.0, 0.1, 200.0);

                glMatrixMode(GL_MODELVIEW); glLoadIdentity();
                float radPitch = camPitch * 3.14159f / 180.0f;
                float lookX = cos(radPitch) * sin(radYaw), lookY = sin(radPitch), lookZ = -cos(radPitch) * cos(radYaw);
                gluLookAt(player.x, player.y, player.z,
                    player.x + lookX, player.y + lookY, player.z + lookZ,
                    0, 1, 0);

                DrawBaseplate();
                for (const auto& b : blocks) DrawSoftShadow(b, (float*)lightDirection);
                for (const auto& b : blocks) DrawBlock(b);
                DrawText2D("FPS: " + std::to_string(int(fps)), 700, 20);

                SwapBuffers(hdc);
            }
        }
    }

    wglMakeCurrent(NULL, NULL); wglDeleteContext(glrc); ReleaseDC(hwnd, hdc);
    return 0;
}
