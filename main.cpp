#include <SFML/Graphics.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <limits>
#include <optional>
#include <system_error>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

using namespace std;

struct User {
    int id;
    string login;
    string password;
};

struct Quiz {
    int id;
    int userId;
    string title;
    string category;
    string theme;
    string bonus;
    int opens;
};

struct Question {
    int id;
    int quizId;
    string text;
    string type;
    string imagePath;
    int timeLimit;
};

struct Answer {
    int id;
    int questionId;
    string text;
    int score;
};

struct Lead {
    int id;
    int quizId;
    string name;
    string phone;
    int score;
    string date;
};

const string DATA_DIR = "data";
const string REPORTS_DIR = DATA_DIR + "/reports";
const string USERS_FILE = DATA_DIR + "/users.txt";
const string QUIZZES_FILE = DATA_DIR + "/quizzes.txt";
const string QUESTIONS_FILE = DATA_DIR + "/questions.txt";
const string ANSWERS_FILE = DATA_DIR + "/answers.txt";
const string LEADS_FILE = DATA_DIR + "/leads.txt";
const size_t MAX_TEXT_LEN = 120;
const size_t MIN_LOGIN_LEN = 3;
const size_t MIN_PASSWORD_LEN = 4;
const int MIN_SCORE = -100000;
const int MAX_SCORE = 100000;
const int MIN_ANSWER_COUNT = 1;
const int MAX_ANSWER_COUNT = 10;
const int MAX_TIME_LIMIT = 3600;

string toUtf8(const u32string& text) {
    string result;
    for (char32_t c : text) {
        if (c <= 0x7F) {
            result.push_back(static_cast<char>(c));
        } else if (c <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | (c >> 6)));
            result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0 | (c >> 12)));
            result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        } else if (c <= 0x10FFFF) {
            result.push_back(static_cast<char>(0xF0 | (c >> 18)));
            result.push_back(static_cast<char>(0x80 | ((c >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (c & 0x3F)));
        }
    }
    return result;
}
sf::String sfText(const string& text) {
    return sf::String::fromUtf8(text.begin(), text.end());
}

string trim(const string& text) {
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == string::npos) {
        return "";
    }

    size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

string lowerText(string text) {
    transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return static_cast<char>(tolower(c));
    });

    return text;
}

string limitText(string text, size_t maxLen = MAX_TEXT_LEN) {
    text = trim(text);
    if (text.size() > maxLen) {
        text.resize(maxLen);
    }

    return text;
}

string cleanForFile(string text) {
    for (char& c : text) {
        if (c == '|') {
            c = '/';
        }

        if (c == '\r' || c == '\n') {
            c = ' ';
        }
    }

    return limitText(text);
}

string htmlEscape(const string& text) {
    string result;

    for (char c : text) {
        if (c == '&') {
            result += "&amp;";
        } else if (c == '<') {
            result += "&lt;";
        } else if (c == '>') {
            result += "&gt;";
        } else if (c == '"') {
            result += "&quot;";
        } else {
            result.push_back(c);
        }
    }

    return result;
}

int countDigits(const string& text) {
    int count = 0;

    for (unsigned char c : text) {
        if (isdigit(c)) {
            count++;
        }
    }

    return count;
}

bool isPhoneTextCorrect(const string& phone) {
    if (phone.size() < 5) {
        return false;
    }

    return countDigits(phone) >= 5;
}

bool parseInt(const string& text,
              int& value,
              int minValue = numeric_limits<int>::min(),
              int maxValue = numeric_limits<int>::max()) {
    string s = trim(text);
    if (s.empty()) {
        return false;
    }

    try {
        size_t pos = 0;
        long long parsed = stoll(s, &pos);

        if (pos != s.size() || parsed < minValue || parsed > maxValue) {
            return false;
        }

        value = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

vector<string> splitLine(const string& line, char ch) {
    vector<string> parts;
    string part;
    stringstream ss(line);

    while (getline(ss, part, ch)) {
        parts.push_back(part);
    }

    return parts;
}

string nowDate() {
    time_t t = time(nullptr);
    tm* lt = localtime(&t);
    if (!lt) {
        return "дата не определена";
    }

    char buf[32];
    strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M", lt);
    return buf;
}

bool ensureDataFiles() {
    error_code ec;
    filesystem::create_directories(REPORTS_DIR, ec);
    if (ec) {
        return false;
    }

    vector<string> files = {USERS_FILE, QUIZZES_FILE, QUESTIONS_FILE, ANSWERS_FILE, LEADS_FILE};
    for (const string& name : files) {
        ofstream file(name, ios::app);
        if (!file) {
            return false;
        }
    }

    return true;
}

bool saveTextFile(const string& fileName, const string& text) {
    ofstream file(fileName, ios::trunc);
    if (!file) {
        return false;
    }

    file << text;
    return (bool)file;
}

vector<User> loadUsers() {
    vector<User> items;
    ifstream file(USERS_FILE);
    string line;

    while (getline(file, line)) {
        auto p = splitLine(line, '|');
        int id = 0;

        if (p.size() == 3 && parseInt(p[0], id, 1)) {
            items.push_back({id, p[1], p[2]});
        }
    }

    return items;
}

vector<Quiz> loadQuizzes() {
    vector<Quiz> items;
    ifstream file(QUIZZES_FILE);
    string line;

    while (getline(file, line)) {
        auto p = splitLine(line, '|');
        int id = 0;
        int userId = 0;
        int opens = 0;

        if (p.size() == 7 &&
            parseInt(p[0], id, 1) &&
            parseInt(p[1], userId, 1) &&
            parseInt(p[6], opens, 0)) {
            items.push_back({id, userId, p[2], p[3], p[4], p[5], opens});
        }
    }

    return items;
}

vector<Question> loadQuestions() {
    vector<Question> items;
    ifstream file(QUESTIONS_FILE);
    string line;

    while (getline(file, line)) {
        auto p = splitLine(line, '|');
        int id = 0;
        int quizId = 0;

        if ((p.size() == 4 || p.size() == 6) &&
            parseInt(p[0], id, 1) &&
            parseInt(p[1], quizId, 1)) {
            int timeLimit = 0;
            string imagePath;

            if (p.size() == 6) {
                imagePath = p[4];
                parseInt(p[5], timeLimit, 0, MAX_TIME_LIMIT);
            }

            items.push_back({id, quizId, p[2], p[3], imagePath, timeLimit});
        }
    }

    return items;
}

vector<Answer> loadAnswers() {
    vector<Answer> items;
    ifstream file(ANSWERS_FILE);
    string line;

    while (getline(file, line)) {
        auto p = splitLine(line, '|');
        int id = 0;
        int questionId = 0;
        int score = 0;

        if (p.size() == 4 &&
            parseInt(p[0], id, 1) &&
            parseInt(p[1], questionId, 1) &&
            parseInt(p[3], score, MIN_SCORE, MAX_SCORE)) {
            items.push_back({id, questionId, p[2], score});
        }
    }

    return items;
}

vector<Lead> loadLeads() {
    vector<Lead> items;
    ifstream file(LEADS_FILE);
    string line;

    while (getline(file, line)) {
        auto p = splitLine(line, '|');
        int id = 0;
        int quizId = 0;
        int score = 0;

        if (p.size() == 6 &&
            parseInt(p[0], id, 1) &&
            parseInt(p[1], quizId, 1) &&
            parseInt(p[4], score, MIN_SCORE, MAX_SCORE)) {
            items.push_back({id, quizId, p[2], p[3], score, p[5]});
        }
    }

    return items;
}

bool saveUsers(const vector<User>& items) {
    stringstream ss;

    for (auto& x : items) {
        ss << x.id << '|' << x.login << '|' << x.password << "\n";
    }

    return saveTextFile(USERS_FILE, ss.str());
}

bool saveQuizzes(const vector<Quiz>& items) {
    stringstream ss;

    for (auto& x : items) {
        ss << x.id << '|' << x.userId << '|' << x.title << '|'
           << x.category << '|' << x.theme << '|' << x.bonus << '|'
           << x.opens << "\n";
    }

    return saveTextFile(QUIZZES_FILE, ss.str());
}

bool saveQuestions(const vector<Question>& items) {
    stringstream ss;

    for (auto& x : items) {
        ss << x.id << '|' << x.quizId << '|' << x.text << '|'
           << x.type << '|' << x.imagePath << '|' << x.timeLimit << "\n";
    }

    return saveTextFile(QUESTIONS_FILE, ss.str());
}

bool saveAnswers(const vector<Answer>& items) {
    stringstream ss;

    for (auto& x : items) {
        ss << x.id << '|' << x.questionId << '|' << x.text << '|'
           << x.score << "\n";
    }

    return saveTextFile(ANSWERS_FILE, ss.str());
}

bool saveLeads(const vector<Lead>& items) {
    stringstream ss;

    for (auto& x : items) {
        ss << x.id << '|' << x.quizId << '|' << x.name << '|'
           << x.phone << '|' << x.score << '|' << x.date << "\n";
    }

    return saveTextFile(LEADS_FILE, ss.str());
}

template <class T>
int nextId(const vector<T>& items) {
    int maxId = 0;

    for (auto& x : items) {
        maxId = max(maxId, x.id);
    }

    return maxId + 1;
}

struct Button {
    sf::FloatRect rect;
    string text;
    string id;
    sf::Color color;
};

struct InputField {
    sf::FloatRect rect;
    string hint;
    u32string text;
    bool active = false;
};

sf::FloatRect makeRect(float x, float y, float w, float h) {
    return sf::FloatRect({x, y}, {w, h});
}

enum class Screen { Login, Home, Quizzes, Builder, QuizStart, Preview, Leads, Reports, Help };

class MarquizApp {
private:
    sf::RenderWindow window;
    sf::Font font;
    vector<Button> buttons;
    vector<InputField*> fields;
    sf::Vector2f mousePos{0, 0};

    vector<User> users;
    vector<Quiz> quizzes;
    vector<Question> questions;
    vector<Answer> answers;
    vector<Lead> leads;

    Screen screen = Screen::Login;
    int currentUserId = -1;
    int selectedQuizId = -1;
    int editingQuestionId = -1;
    string message = "Введите логин и пароль.";
    string lastMessage;
    chrono::steady_clock::time_point messageChangedAt = chrono::steady_clock::now();
    size_t previewIndex = 0;
    int previewScore = 0;
    bool contactStep = false;

    InputField loginField{makeRect(380, 220, 420, 44), "логин"};
    InputField passField{makeRect(380, 282, 420, 44), "пароль"};
    InputField titleField{makeRect(300, 175, 260, 42), "название квиза"};
    InputField categoryField{makeRect(580, 175, 210, 42), "категория"};
    InputField themeField{makeRect(810, 175, 210, 42), "тема"};
    InputField bonusField{makeRect(300, 235, 350, 42), "бонус/оффер после прохождения"};
    InputField questionField{makeRect(300, 175, 420, 42), "текст вопроса"};
    InputField imagePathField{makeRect(300, 235, 260, 42), "путь к картинке (необязательно)"};
    InputField timeLimitField{makeRect(575, 235, 80, 42), "сек"};
    vector<InputField> answerFields;
    vector<InputField> scoreFields;
    InputField leadNameField{makeRect(300, 420, 260, 42), "имя клиента"};
    InputField leadPhoneField{makeRect(580, 420, 260, 42), "телефон"};
    InputField searchField{makeRect(300, 170, 360, 42), "поиск по заявкам"};
    InputField previewTextField{makeRect(300, 420, 360, 44), "ваш ответ"};
    string questionType = "варианты";
    int answerCount = 2;

    // Цвета интерфейса.
    const sf::Color bg{10, 12, 13};
    const sf::Color sidebar{18, 22, 23};
    const sf::Color card{22, 27, 28};
    const sf::Color cardSoft{29, 36, 37};
    const sf::Color field{14, 17, 18};
    const sf::Color fieldActive{23, 29, 30};
    const sf::Color accent{123, 151, 132};
    const sf::Color violet{74, 105, 96};
    const sf::Color violetSoft{29, 45, 42};
    const sf::Color danger{164, 82, 75};
    const sf::Color text{230, 232, 225};
    const sf::Color muted{143, 151, 145};
    const sf::Color line{54, 63, 61};
public:
    MarquizApp()
        : window(sf::VideoMode({1100, 720}),
                 sfText("Квиз"),
                 sf::Style::Default,
                 sf::State::Windowed,
                 makeSettings()) {
        window.setFramerateLimit(60);
        for (int i = 0; i < MAX_ANSWER_COUNT; i++) {
            answerFields.push_back({makeRect(0, 0, 260, 42), "ответ " + to_string(i + 1)});
            scoreFields.push_back({makeRect(0, 0, 80, 42), "баллы"});
        }
        setupWorkingDirectory();

        if (!ensureDataFiles()) {
            message = "Ошибка: не удалось подготовить папку data.";
        }

        users = loadUsers();
        quizzes = loadQuizzes();
        questions = loadQuestions();
        answers = loadAnswers();
        leads = loadLeads();

        if (!loadFont()) {
            cout << "Не удалось загрузить шрифт\n";
        }
    }

    void run() {
        while (window.isOpen()) {
            handleEvents();
            buttons.clear();
            fields.clear();
            window.clear(bg);
            drawBackground();
            drawScreen();
            window.display();
        }
    }

private:
    static sf::ContextSettings makeSettings() {
        sf::ContextSettings settings;
        settings.antiAliasingLevel = 8;
        return settings;
    }

    float appW() const {
        return (float)window.getSize().x;
    }

    float appH() const {
        return (float)window.getSize().y;
    }

    float contentX() const {
        if (currentUserId == -1) {
            return max(320.f, (appW() - 420.f) / 2.f);
        }

        return 64.f;
    }

    float contentW() const {
        float x = currentUserId == -1 ? contentX() : 64.f;
        return max(420.f, appW() - x - 64.f);
    }

    float footerY() const {
        return max(620.f, appH() - 58.f);
    }

    float snap(float v) const {
        return round(v);
    }

    bool hovered(sf::FloatRect r) const {
        return r.contains(mousePos);
    }

    sf::Color mix(sf::Color c, int d) const {
        auto clampColor = [](int value) {
            return (uint8_t)max(0, min(255, value));
        };

        return sf::Color(
            clampColor(c.r + d),
            clampColor(c.g + d),
            clampColor(c.b + d),
            c.a
        );
    }

    bool loadFont() {
        vector<string> paths;
#ifdef _WIN32
        paths = {"C:/Windows/Fonts/segoeui.ttf", "C:/Windows/Fonts/arial.ttf", "C:/Windows/Fonts/tahoma.ttf"};
#elif defined(__APPLE__)
        paths = {"/System/Library/Fonts/SFNS.ttf", "/System/Library/Fonts/Helvetica.ttc", "/System/Library/Fonts/Supplemental/Arial Unicode.ttf"};
#else
        paths = {"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"};
#endif
        for (auto& p : paths) {
            if (font.openFromFile(p)) {
                return true;
            }
        }

        return false;
    }

    void setupWorkingDirectory() {
        error_code ec;
#ifdef __APPLE__
        char buf[4096];
        uint32_t size = sizeof(buf);

        if (_NSGetExecutablePath(buf, &size) == 0) {
            filesystem::path folder = filesystem::weakly_canonical(buf, ec).parent_path();

            if (!ec && folder.string().find(".app/Contents/MacOS") != string::npos) {
                folder = folder.parent_path().parent_path().parent_path();
            }

            if (!ec) {
                filesystem::current_path(folder, ec);
            }
        }
#elif defined(_WIN32)
        wchar_t buf[MAX_PATH];
        DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);

        if (len > 0) {
            filesystem::current_path(filesystem::path(buf).parent_path(), ec);
        }
#endif
    }

    void handleEvents() {
        while (const optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }

            if (auto* e = event->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect(
                    {0, 0},
                    {(float)e->size.x, (float)e->size.y}
                )));
            }

            if (auto* e = event->getIf<sf::Event::MouseMoved>()) {
                mousePos = {(float)e->position.x, (float)e->position.y};
            }

            if (auto* e = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (e->button == sf::Mouse::Button::Left) {
                    mousePos = {(float)e->position.x, (float)e->position.y};
                    handleMouse(mousePos);
                }
            }

            if (auto* e = event->getIf<sf::Event::KeyPressed>()) {
                if (e->code == sf::Keyboard::Key::F1) {
                    screen = Screen::Help;
                }

                if (e->code == sf::Keyboard::Key::Escape && currentUserId != -1) {
                    screen = Screen::Home;
                }
            }

            if (auto* e = event->getIf<sf::Event::TextEntered>()) {
                handleText(e->unicode);
            }
        }
    }

    void handleMouse(sf::Vector2f p) {
        for (auto* f : fields) {
            f->active = f->rect.contains(p);
        }

        for (auto& b : buttons) {
            if (b.rect.contains(p)) {
                click(b.id);
                return;
            }
        }
    }

    void handleText(char32_t c) {
        for (auto* f : fields) {
            if (!f->active) {
                continue;
            }

            if (c == 8) {
                if (!f->text.empty()) {
                    f->text.pop_back();
                }
            } else if (c == 13 || c == 10) {
                f->active = false;
            } else if (c >= 32 && f->text.size() < MAX_TEXT_LEN) {
                f->text.push_back(c);
            }
        }
    }

    vector<Quiz> userQuizzes() const {
        vector<Quiz> result;

        for (auto& q : quizzes) {
            if (q.userId == currentUserId) {
                result.push_back(q);
            }
        }

        return result;
    }

    vector<Question> quizQuestions(int quizId) const {
        vector<Question> result;

        for (auto& q : questions) {
            if (q.quizId == quizId) {
                result.push_back(q);
            }
        }

        return result;
    }

    vector<Answer> questionAnswers(int questionId) const {
        vector<Answer> result;

        for (auto& a : answers) {
            if (a.questionId == questionId) {
                result.push_back(a);
            }
        }

        return result;
    }

    int leadCountForQuiz(int quizId) const {
        int count = 0;

        for (auto& l : leads) {
            if (l.quizId == quizId) {
                count++;
            }
        }

        return count;
    }

    int questionCountForQuiz(int quizId) const {
        int count = 0;

        for (auto& q : questions) {
            if (q.quizId == quizId) {
                count++;
            }
        }

        return count;
    }

    bool quizHasAnswers(int quizId) const {
        auto quizQuestionList = quizQuestions(quizId);

        if (quizQuestionList.empty()) {
            return false;
        }

        for (auto& q : quizQuestionList) {
            if (questionAnswers(q.id).empty()) {
                return false;
            }
        }

        return true;
    }

    string selectedQuizStatus() const {
        if (!quizExists(selectedQuizId)) {
            return "Квиз не выбран";
        }

        return quizTitle(selectedQuizId) +
            " | вопросов: " + to_string(questionCountForQuiz(selectedQuizId)) +
            " | заявок: " + to_string(leadCountForQuiz(selectedQuizId));
    }

    string quizTitle(int id) const {
        for (auto& q : quizzes) {
            if (q.id == id) {
                return q.title;
            }
        }

        return "Квиз не выбран";
    }

    bool quizExists(int id) const {
        for (auto& q : quizzes) {
            if (q.id == id && q.userId == currentUserId) {
                return true;
            }
        }

        return false;
    }

    void selectFirstQuizForCurrentUser() {
        selectedQuizId = -1;

        for (auto& q : quizzes) {
            if (q.userId == currentUserId) {
                selectedQuizId = q.id;
                return;
            }
        }
    }

    bool readButtonId(const string& id, const string& prefix, int& value) const {
        if (id.rfind(prefix, 0) != 0) {
            return false;
        }

        return parseInt(id.substr(prefix.size()), value, 1);
    }

    void click(const string& id) {
        int objectId = 0;

        if (id == "login") {
            login();
        } else if (id == "register") {
            registerUser();
        } else if (id == "logout") {
            logout();
        } else if (id == "home") {
            screen = Screen::Home;
        } else if (id == "quizzes") {
            screen = Screen::Quizzes;
        } else if (id == "builder") {
            screen = Screen::Builder;
        } else if (id == "quizStart") {
            screen = Screen::QuizStart;
        } else if (id == "leads") {
            screen = Screen::Leads;
        } else if (id == "reports") {
            screen = Screen::Reports;
        } else if (id == "help") {
            screen = Screen::Help;
        } else if (id == "addQuiz") {
            addQuiz();
        } else if (id == "addQuestion") {
            addQuestion();
        } else if (id == "cancelQuestionEdit") {
            clearQuestionForm();
            message = "Редактирование отменено.";
        } else if (id == "answerMinus") {
            answerCount = max(MIN_ANSWER_COUNT, answerCount - 1);
            message = "Количество ответов: " + to_string(answerCount);
        } else if (id == "answerPlus") {
            answerCount = min(MAX_ANSWER_COUNT, answerCount + 1);
            message = "Количество ответов: " + to_string(answerCount);
        } else if (id == "typeVariants") {
            questionType = "варианты";
            answerCount = max(2, answerCount);
            message = "Формат вопроса: варианты ответа.";
        } else if (id == "typeText") {
            questionType = "текст";
            answerCount = 1;
            message = "Формат вопроса: текстовый ответ.";
        } else if (id == "typeMatch") {
            questionType = "соответствие";
            answerCount = max(2, answerCount);
            message = "Формат вопроса: соответствие.";
        } else if (id == "demo") {
            addDemo();
        } else if (id == "saveLead") {
            saveLead();
        } else if (id == "submitTextAnswer") {
            submitTextAnswer();
        } else if (id == "report") {
            makeReport();
        } else if (readButtonId(id, "selectQuiz_", objectId)) {
            if (!quizExists(objectId)) {
                message = "Квиз не найден.";
                return;
            }

            selectedQuizId = objectId;
            screen = Screen::Builder;
            message = "Выбран квиз: " + quizTitle(selectedQuizId);
        } else if (readButtonId(id, "deleteQuiz_", objectId)) {
            deleteQuiz(objectId);
        } else if (readButtonId(id, "startQuiz_", objectId)) {
            if (!quizExists(objectId)) {
                message = "Квиз не найден.";
                return;
            }

            selectedQuizId = objectId;
            startPreview();
        } else if (readButtonId(id, "editQuestion_", objectId)) {
            loadQuestionToForm(objectId);
        } else if (readButtonId(id, "deleteQuestion_", objectId)) {
            deleteQuestion(objectId);
        } else if (readButtonId(id, "leadCert_", objectId)) {
            makeCertificate(objectId);
        } else if (readButtonId(id, "deleteLead_", objectId)) {
            deleteLead(objectId);
        } else if (readButtonId(id, "answer_", objectId)) {
            chooseAnswer(objectId);
        }
    }

    void login() {
        string login = cleanForFile(toUtf8(loginField.text));
        string password = cleanForFile(toUtf8(passField.text));

        if (login.empty() || password.empty()) {
            message = "Введите логин и пароль.";
            return;
        }

        for (auto& u : users) {
            if (u.login == login && u.password == password) {
                currentUserId = u.id;
                selectFirstQuizForCurrentUser();
                screen = Screen::Home;
                message.clear();
                return;
            }
        }

        message = "Неверный логин или пароль.";
    }

    void registerUser() {
        string login = cleanForFile(toUtf8(loginField.text));
        string password = cleanForFile(toUtf8(passField.text));

        if (login.empty() || password.empty()) {
            message = "Заполните логин и пароль.";
            return;
        }

        if (login.size() < MIN_LOGIN_LEN) {
            message = "Логин должен быть не короче 3 символов.";
            return;
        }

        if (password.size() < MIN_PASSWORD_LEN) {
            message = "Пароль должен быть не короче 4 символов.";
            return;
        }

        if (login.find(' ') != string::npos || password.find(' ') != string::npos) {
            message = "Логин и пароль не должны содержать пробелы.";
            return;
        }

        for (auto& u : users) {
            if (u.login == login) {
                message = "Такой логин уже существует.";
                return;
            }
        }

        int id = nextId(users);
        users.push_back({id, login, password});

        if (!saveUsers(users)) {
            users.pop_back();
            message = "Ошибка записи файла пользователей.";
            return;
        }

        currentUserId = id;
        screen = Screen::Home;
        message.clear();
    }

    void logout() {
        currentUserId = -1;
        selectedQuizId = -1;
        screen = Screen::Login;
        message = "Введите логин и пароль.";
        loginField.text.clear();
        passField.text.clear();
    }

    void addQuiz() {
        string title = cleanForFile(toUtf8(titleField.text));
        string category = cleanForFile(toUtf8(categoryField.text));
        string theme = cleanForFile(toUtf8(themeField.text));
        string bonus = cleanForFile(toUtf8(bonusField.text));

        if (title.empty() || category.empty() || theme.empty()) {
            message = "Заполните название, категорию и тему.";
            return;
        }

        for (auto& q : quizzes) {
            if (q.userId == currentUserId && lowerText(q.title) == lowerText(title)) {
                message = "Квиз с таким названием уже есть.";
                return;
            }
        }

        if (bonus.empty()) {
            bonus = "Бонус не указан";
        }

        int id = nextId(quizzes);
        quizzes.push_back({id, currentUserId, title, category, theme, bonus, 0});

        if (!saveQuizzes(quizzes)) {
            quizzes.pop_back();
            message = "Ошибка записи файла квизов.";
            return;
        }

        selectedQuizId = id;
        titleField.text.clear();
        categoryField.text.clear();
        themeField.text.clear();
        bonusField.text.clear();
        message = "Квиз создан.";
    }

    void deleteQuiz(int quizId) {
        if (!quizExists(quizId)) {
            message = "Квиз не найден.";
            return;
        }

        auto oldQuizzes = quizzes;
        auto oldQuestions = questions;
        auto oldAnswers = answers;
        auto oldLeads = leads;

        vector<int> questionIds;
        for (auto& q : questions) {
            if (q.quizId == quizId) {
                questionIds.push_back(q.id);
            }
        }

        quizzes.erase(remove_if(quizzes.begin(), quizzes.end(), [&](const Quiz& q) {
            return q.id == quizId && q.userId == currentUserId;
        }), quizzes.end());

        questions.erase(remove_if(questions.begin(), questions.end(), [&](const Question& q) {
            return q.quizId == quizId;
        }), questions.end());

        answers.erase(remove_if(answers.begin(), answers.end(), [&](const Answer& a) {
            return find(questionIds.begin(), questionIds.end(), a.questionId) != questionIds.end();
        }), answers.end());

        leads.erase(remove_if(leads.begin(), leads.end(), [&](const Lead& l) {
            return l.quizId == quizId;
        }), leads.end());

        if (!saveQuizzes(quizzes) || !saveQuestions(questions) || !saveAnswers(answers) || !saveLeads(leads)) {
            quizzes = oldQuizzes;
            questions = oldQuestions;
            answers = oldAnswers;
            leads = oldLeads;
            saveQuizzes(quizzes);
            saveQuestions(questions);
            saveAnswers(answers);
            saveLeads(leads);
            message = "Ошибка удаления квиза.";
            return;
        }

        if (selectedQuizId == quizId) {
            selectFirstQuizForCurrentUser();
        }

        screen = Screen::Quizzes;
        message = "Квиз удален.";
    }

    void addQuestion() {
        if (!quizExists(selectedQuizId)) {
            message = "Сначала выберите квиз.";
            return;
        }

        string question = cleanForFile(toUtf8(questionField.text));
        string imagePath = cleanForFile(toUtf8(imagePathField.text));
        string timeLimitText = toUtf8(timeLimitField.text);
        int timeLimit = 0;

        if (!trim(timeLimitText).empty() && !parseInt(timeLimitText, timeLimit, 0, MAX_TIME_LIMIT)) {
            message = "Время должно быть числом от 0 до 3600.";
            return;
        }

        if (question.empty()) {
            message = "Заполните текст вопроса.";
            return;
        }

        vector<string> answerTexts;
        vector<int> answerScores;

        for (int i = 0; i < answerCount; i++) {
            string answer = cleanForFile(toUtf8(answerFields[i].text));
            string scoreText = toUtf8(scoreFields[i].text);
            int score = 0;

            if (answer.empty()) {
                message = "Заполните все ответы.";
                return;
            }

            if (questionType == "соответствие" && answer.find("->") == string::npos) {
                message = "Для соответствия используйте формат: слово -> пара.";
                return;
            }

            if (!trim(scoreText).empty() && !parseInt(scoreText, score, MIN_SCORE, MAX_SCORE)) {
                message = "Баллы ответа " + to_string(i + 1) + " должны быть числом.";
                return;
            }

            for (auto& existing : answerTexts) {
                if (lowerText(existing) == lowerText(answer)) {
                    message = "Ответы не должны повторяться.";
                    return;
                }
            }

            answerTexts.push_back(answer);
            answerScores.push_back(score);
        }

        if (questionType == "варианты" && answerTexts.size() < 2) {
            message = "Для теста нужно минимум два варианта.";
            return;
        }

        auto oldQuestions = questions;
        auto oldAnswers = answers;
        int questionId = editingQuestionId == -1 ? nextId(questions) : editingQuestionId;

        if (editingQuestionId == -1) {
            questions.push_back({questionId, selectedQuizId, question, questionType, imagePath, timeLimit});
        } else {
            bool found = false;
            for (auto& q : questions) {
                if (q.id == editingQuestionId && q.quizId == selectedQuizId) {
                    q.text = question;
                    q.type = questionType;
                    q.imagePath = imagePath;
                    q.timeLimit = timeLimit;
                    found = true;
                    break;
                }
            }

            if (!found) {
                message = "Вопрос для изменения не найден.";
                return;
            }

            answers.erase(remove_if(answers.begin(), answers.end(), [&](const Answer& a) {
                return a.questionId == editingQuestionId;
            }), answers.end());
        }

        for (size_t i = 0; i < answerTexts.size(); i++) {
            answers.push_back({nextId(answers), questionId, answerTexts[i], answerScores[i]});
        }

        // Сохраняем связанные файлы вместе. При ошибке возвращаем старое состояние.
        if (!saveQuestions(questions) || !saveAnswers(answers)) {
            questions = oldQuestions;
            answers = oldAnswers;
            saveQuestions(questions);
            saveAnswers(answers);
            message = "Ошибка записи вопроса.";
            return;
        }

        bool wasEditing = editingQuestionId != -1;
        clearQuestionForm();
        message = wasEditing ? "Вопрос изменен." : "Вопрос добавлен.";
    }

    void clearQuestionForm() {
        editingQuestionId = -1;
        questionField.text.clear();
        imagePathField.text.clear();
        timeLimitField.text.clear();
        questionType = "варианты";
        answerCount = 2;

        for (int i = 0; i < MAX_ANSWER_COUNT; i++) {
            answerFields[i].text.clear();
            scoreFields[i].text.clear();
        }
    }

    void loadQuestionToForm(int questionId) {
        for (auto& q : questions) {
            if (q.id == questionId && q.quizId == selectedQuizId) {
                editingQuestionId = q.id;
                questionField.text = sfText(q.text).toUtf32();
                imagePathField.text = sfText(q.imagePath).toUtf32();
                timeLimitField.text = sfText(q.timeLimit > 0 ? to_string(q.timeLimit) : "").toUtf32();
                questionType = q.type;

                auto list = questionAnswers(q.id);
                answerCount = max(MIN_ANSWER_COUNT, min(MAX_ANSWER_COUNT, (int)list.size()));

                for (int i = 0; i < MAX_ANSWER_COUNT; i++) {
                    answerFields[i].text.clear();
                    scoreFields[i].text.clear();
                }

                for (int i = 0; i < answerCount; i++) {
                    answerFields[i].text = sfText(list[i].text).toUtf32();
                    scoreFields[i].text = sfText(to_string(list[i].score)).toUtf32();
                }

                message = "Вопрос открыт для изменения.";
                return;
            }
        }

        message = "Вопрос не найден.";
    }

    void deleteQuestion(int questionId) {
        bool belongsToSelectedQuiz = false;
        for (auto& q : questions) {
            if (q.id == questionId && q.quizId == selectedQuizId) {
                belongsToSelectedQuiz = true;
                break;
            }
        }

        if (!belongsToSelectedQuiz) {
            message = "Вопрос не найден.";
            return;
        }

        auto oldQuestions = questions;
        auto oldAnswers = answers;

        questions.erase(remove_if(questions.begin(), questions.end(), [&](const Question& q) {
            return q.id == questionId;
        }), questions.end());

        answers.erase(remove_if(answers.begin(), answers.end(), [&](const Answer& a) {
            return a.questionId == questionId;
        }), answers.end());

        if (!saveQuestions(questions) || !saveAnswers(answers)) {
            questions = oldQuestions;
            answers = oldAnswers;
            saveQuestions(questions);
            saveAnswers(answers);
            message = "Ошибка удаления вопроса.";
            return;
        }

        if (editingQuestionId == questionId) {
            clearQuestionForm();
        }

        message = "Вопрос удален.";
    }

    void addDemo() {
        auto oldQuizzes = quizzes;
        auto oldQuestions = questions;
        auto oldAnswers = answers;

        int quizId = nextId(quizzes);
        int question1Id = nextId(questions);
        int question2Id = question1Id + 1;

        quizzes.push_back({
            quizId,
            currentUserId,
            "Подбор курса программирования",
            "Образование",
            "Профориентация",
            "Скидка 10% на первый месяц",
            0
        });

        questions.push_back({question1Id, quizId, "Какой формат обучения вам удобнее?", "варианты", "", 0});
        answers.push_back({nextId(answers), question1Id, "Онлайн", 5});
        answers.push_back({nextId(answers), question1Id, "Очно", 3});

        questions.push_back({question2Id, quizId, "Какой уровень подготовки?", "варианты", "", 0});
        answers.push_back({nextId(answers), question2Id, "Начинающий", 2});
        answers.push_back({nextId(answers), question2Id, "Уже писал код", 6});

        if (!saveQuizzes(quizzes) || !saveQuestions(questions) || !saveAnswers(answers)) {
            quizzes = oldQuizzes;
            questions = oldQuestions;
            answers = oldAnswers;
            saveQuizzes(quizzes);
            saveQuestions(questions);
            saveAnswers(answers);
            message = "Ошибка записи демо-данных.";
            return;
        }

        selectedQuizId = quizId;
        message = "Демо-квиз добавлен.";
    }

    void startPreview() {
        if (!quizExists(selectedQuizId)) {
            message = "Сначала выберите квиз.";
            screen = Screen::Quizzes;
            return;
        }

        auto quizQuestionList = quizQuestions(selectedQuizId);
        if (quizQuestionList.empty()) {
            message = "В квизе пока нет вопросов.";
            return;
        }

        if (!quizHasAnswers(selectedQuizId)) {
            message = "У каждого вопроса должен быть хотя бы один ответ.";
            return;
        }

        for (auto& q : quizzes) {
            if (q.id == selectedQuizId) {
                q.opens++;
            }
        }

        if (!saveQuizzes(quizzes)) {
            for (auto& q : quizzes) {
                if (q.id == selectedQuizId && q.opens > 0) {
                    q.opens--;
                }
            }

            message = "Ошибка записи статистики.";
            return;
        }

        previewIndex = 0;
        previewScore = 0;
        contactStep = false;
        leadNameField.text.clear();
        leadPhoneField.text.clear();
        screen = Screen::Preview;
        message = "Предпросмотр квиза запущен.";
    }

    void chooseAnswer(int answerId) {
        if (!quizExists(selectedQuizId) || contactStep) {
            return;
        }

        auto quizQuestionList = quizQuestions(selectedQuizId);
        if (previewIndex >= quizQuestionList.size()) {
            contactStep = true;
            return;
        }

        bool found = false;

        for (auto& a : questionAnswers(quizQuestionList[previewIndex].id)) {
            if (a.id == answerId) {
                previewScore += a.score;
                found = true;
                break;
            }
        }

        if (!found) {
            message = "Вариант ответа не найден.";
            return;
        }

        previewIndex++;

        if (previewIndex >= quizQuestionList.size()) {
            contactStep = true;
            message = "Квиз пройден. Оставьте контакт.";
        }
    }

    void submitTextAnswer() {
        if (!quizExists(selectedQuizId) || contactStep) {
            return;
        }

        auto quizQuestionList = quizQuestions(selectedQuizId);
        if (previewIndex >= quizQuestionList.size()) {
            contactStep = true;
            return;
        }

        auto currentQuestion = quizQuestionList[previewIndex];
        auto currentAnswers = questionAnswers(currentQuestion.id);
        string value = lowerText(cleanForFile(toUtf8(previewTextField.text)));

        if (value.empty()) {
            message = "Введите ответ.";
            return;
        }

        for (auto& answer : currentAnswers) {
            if (lowerText(answer.text) == value) {
                previewScore += answer.score;
                break;
            }
        }

        previewTextField.text.clear();
        previewIndex++;

        if (previewIndex >= quizQuestionList.size()) {
            contactStep = true;
            message = "Квиз пройден. Оставьте контакт.";
        }
    }

    void saveLead() {
        if (!quizExists(selectedQuizId) || !contactStep) {
            message = "Сначала пройдите квиз.";
            return;
        }

        string name = cleanForFile(toUtf8(leadNameField.text));
        string phone = cleanForFile(toUtf8(leadPhoneField.text));

        if (name.empty() || phone.empty()) {
            message = "Заполните имя и телефон.";
            return;
        }

        if (!isPhoneTextCorrect(phone)) {
            message = "Телефон должен содержать минимум 5 цифр.";
            return;
        }

        leads.push_back({nextId(leads), selectedQuizId, name, phone, previewScore, nowDate()});

        if (!saveLeads(leads)) {
            leads.pop_back();
            message = "Ошибка записи заявки.";
            return;
        }

        screen = Screen::Home;
        message = "Заявка сохранена.";
    }

    void deleteLead(int leadId) {
        bool found = false;
        for (auto& l : leads) {
            if (l.id == leadId && quizExists(l.quizId)) {
                found = true;
                break;
            }
        }

        if (!found) {
            message = "Заявка не найдена.";
            return;
        }

        auto oldLeads = leads;
        leads.erase(remove_if(leads.begin(), leads.end(), [&](const Lead& l) {
            return l.id == leadId;
        }), leads.end());

        if (!saveLeads(leads)) {
            leads = oldLeads;
            saveLeads(leads);
            message = "Ошибка удаления заявки.";
            return;
        }

        message = "Заявка удалена.";
    }

    void makeCertificate(int leadId) {
        const Lead* lead = nullptr;
        for (auto& l : leads) {
            if (l.id == leadId && quizExists(l.quizId)) {
                lead = &l;
                break;
            }
        }

        if (!lead) {
            message = "Заявка не найдена.";
            return;
        }

        error_code ec;
        filesystem::create_directories(REPORTS_DIR, ec);
        if (ec) {
            message = "Ошибка создания папки reports.";
            return;
        }

        string fileName = REPORTS_DIR + "/certificate_" + to_string(lead->id) + ".html";
        ofstream file(fileName, ios::trunc);
        if (!file) {
            message = "Не удалось создать сертификат.";
            return;
        }

        file << "<!doctype html><html><head><meta charset='utf-8'><title>Сертификат</title>";
        file << "<style>body{font-family:Arial,sans-serif;background:#f4f7fb;margin:0;padding:46px;color:#18212f}";
        file << ".cert{max-width:860px;margin:auto;background:white;border:2px solid #2c5bd6;padding:54px;text-align:center}";
        file << "h1{font-size:44px;margin:0 0 28px}.name{font-size:34px;color:#008f7d;margin:24px 0}";
        file << ".score{font-size:24px;margin:26px 0}.meta{color:#5e6f84;margin-top:40px}</style></head><body>";
        file << "<div class='cert'><h1>Сертификат</h1>";
        file << "<p>Подтверждает прохождение квиза</p>";
        file << "<div class='name'>" << htmlEscape(lead->name) << "</div>";
        file << "<p>Квиз: <b>" << htmlEscape(quizTitle(lead->quizId)) << "</b></p>";
        file << "<div class='score'>Результат: " << lead->score << " баллов</div>";
        file << "<p class='meta'>Телефон: " << htmlEscape(lead->phone) << " | Дата: " << htmlEscape(lead->date) << "</p>";
        file << "</div></body></html>";

        if (!file) {
            message = "Ошибка записи сертификата.";
            return;
        }

        message = "Сертификат создан: " + fileName;
    }

    void makeReport() {
        if (!quizExists(selectedQuizId)) {
            message = "Выберите квиз для отчета.";
            return;
        }

        error_code ec;
        filesystem::create_directories(REPORTS_DIR, ec);

        if (ec) {
            message = "Ошибка создания папки reports.";
            return;
        }

        string fileName = REPORTS_DIR + "/quiz_report_" + to_string(selectedQuizId) + ".html";
        ofstream report(fileName, ios::trunc);

        if (!report) {
            message = "Не удалось создать отчет.";
            return;
        }

        int leadCount = leadCountForQuiz(selectedQuizId);
        int questionCount = (int)quizQuestions(selectedQuizId).size();
        int opens = 0;
        string category;
        string theme;
        string bonus;

        for (auto& q : quizzes) {
            if (q.id == selectedQuizId) {
                opens = q.opens;
                category = q.category;
                theme = q.theme;
                bonus = q.bonus;
            }
        }

        report << "<!doctype html><html><head><meta charset='utf-8'><title>Отчет по квизу</title>";
        report << "<style>body{font-family:Arial,sans-serif;background:#f4f7fb;color:#18212f;margin:0;padding:36px}";
        report << "main{max-width:980px;margin:auto;background:#fff;padding:34px;border:1px solid #cbd5e1}";
        report << "h1{margin-top:0}.grid{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;margin:24px 0}";
        report << ".card{background:#e8f1f9;padding:16px;border-radius:8px}.label{color:#5e6f84;font-size:13px}";
        report << "table{width:100%;border-collapse:collapse;margin-top:24px}th,td{border-bottom:1px solid #cbd5e1;padding:12px;text-align:left}";
        report << "th{background:#e8f1f9}</style></head><body><main>";
        report << "<h1>Отчет по квизу</h1>";
        report << "<p><b>" << htmlEscape(quizTitle(selectedQuizId)) << "</b></p>";
        report << "<p>Категория: " << htmlEscape(category) << " | Тема: " << htmlEscape(theme) << "</p>";
        report << "<p>Бонус: " << htmlEscape(bonus) << "</p>";
        report << "<div class='grid'>";
        report << "<div class='card'><div class='label'>Открытий</div><b>" << opens << "</b></div>";
        report << "<div class='card'><div class='label'>Вопросов</div><b>" << questionCount << "</b></div>";
        report << "<div class='card'><div class='label'>Заявок</div><b>" << leadCount << "</b></div>";
        report << "<div class='card'><div class='label'>Конверсия</div><b>" << (opens ? leadCount * 100 / opens : 0) << "%</b></div>";
        report << "</div><h2>Заявки</h2><table><tr><th>Дата</th><th>Имя</th><th>Телефон</th><th>Баллы</th></tr>";

        for (auto& l : leads) {
            if (l.quizId == selectedQuizId) {
                report << "<tr><td>" << htmlEscape(l.date) << "</td><td>" << htmlEscape(l.name) << "</td><td>"
                       << htmlEscape(l.phone) << "</td><td>" << l.score << "</td></tr>";
            }
        }

        report << "</table></main></body></html>";

        if (!report) {
            message = "Ошибка записи отчета.";
            return;
        }

        message = "Отчет создан: " + fileName;
    }

    void drawScreen() {
        if (currentUserId != -1) {
            drawMenu();
        }

        if (screen == Screen::Login) {
            drawLogin();
        } else if (screen == Screen::Home) {
            drawHome();
        } else if (screen == Screen::Quizzes) {
            drawQuizzes();
        } else if (screen == Screen::Builder) {
            drawBuilder();
        } else if (screen == Screen::QuizStart) {
            drawQuizStart();
        } else if (screen == Screen::Preview) {
            drawPreview();
        } else if (screen == Screen::Leads) {
            drawLeads();
        } else if (screen == Screen::Reports) {
            drawReports();
        } else if (screen == Screen::Help) {
            drawHelp();
        }

        drawMessage();
    }

    void drawBackground() {
        drawRect(makeRect(0, 0, appW(), appH()), bg);
        drawRect(makeRect(0, 0, appW(), 92), sf::Color(13, 16, 17));
        drawRect(makeRect(0, 92, appW(), 1), line);
        drawRect(makeRect(0, 93, appW(), 3), sf::Color(20, 26, 26));
    }

    void drawMenu() {
        drawRect(makeRect(0, 0, appW(), 92), sidebar);
        drawRoundedRect(makeRect(34, 30, 6, 32), 2, accent);
        drawText("Квиз", 56, 28, 25, text);

        float buttonY = 28;
        float gap = 8;
        float bx = max(250.f, appW() - 610.f);

        addNavButton("home", "Главная", bx, buttonY, 86, 36, screen == Screen::Home);
        addNavButton("quizzes", "Квизы", bx + 86 + gap, buttonY, 74, 36, screen == Screen::Quizzes);
        addNavButton("builder", "Вопросы", bx + 168 + gap, buttonY, 90, 36, screen == Screen::Builder);
        addNavButton("leads", "Заявки", bx + 266 + gap, buttonY, 80, 36, screen == Screen::Leads);
        addNavButton("reports", "Отчеты", bx + 354 + gap, buttonY, 82, 36, screen == Screen::Reports);
        addButton("logout", "Выход", appW() - 108, buttonY, 74, 36, danger);
    }

    void drawLogin() {
        float cardW = min(520.f, appW() - 80.f);
        float cardH = 398.f;
        float cardX = snap((appW() - cardW) / 2.f);
        float cardY = snap(max(96.f, (appH() - cardH) / 2.f - 18.f));
        float x = cardX + 50.f;
        float fieldW = cardW - 100.f;

        drawSurface(makeRect(cardX, cardY, cardW, cardH), 8, card, line, true);
        drawRoundedRect(makeRect(x, cardY + 42, 72, 4), 1, accent);
        drawText("Вход", x, cardY + 76, 34, text);
        drawText("Квиз", x, cardY + 122, 16, muted);

        loginField.rect = makeRect(x, cardY + 172, fieldW, 46);
        passField.rect = makeRect(x, cardY + 234, fieldW, 46);
        drawInput(loginField);
        drawInput(passField, true);

        float buttonW = (fieldW - 18.f) / 2.f;
        addButton("login", "Войти", x, cardY + 314, buttonW, 44);
        addButton("register", "Регистрация", x + buttonW + 18.f, cardY + 314, buttonW, 44, violet);
        drawText(message, x, cardY + 370, 15, muted);
    }
    void drawHome() {
        float x = contentX();
        float w = min(860.f, contentW());
        int quizCount = (int)userQuizzes().size();
        int userLeadCount = 0;

        for (auto& l : leads) {
            for (auto& q : quizzes) {
                if (q.userId == currentUserId && q.id == l.quizId) {
                    userLeadCount++;
                    break;
                }
            }
        }

        drawText("Панель управления", x, 126, 33, text);
        drawText("Квизы, вопросы, заявки, отчеты.", x, 168, 16, muted);
        drawText("Выбрано: " + selectedQuizStatus(), x, 196, 15, muted);

        drawSurface(makeRect(x, 238, w, 138), 8, card, line, true);
        drawRoundedRect(makeRect(x + 28, 266, 54, 54), 6, accent);
        drawText(to_string(quizCount), x + 46, 278, 25, bg);
        drawText("Квизы", x + 104, 264, 23, text);
        drawText("Создание и настройка квизов.", x + 104, 301, 16, muted);

        drawSurface(makeRect(x, 404, w, 118), 8, card, line, true);
        drawText("Заявки: " + to_string(userLeadCount), x + 28, 432, 22, text);
        drawText("Открытия, заявки и конверсия выбранного квиза.", x + 28, 468, 16, muted);
        addButton("demo", "Добавить демо-квиз", x, 562, 220, 44);
        addButton("quizStart", "Пройти квиз", x + 240, 562, 160, 44, violet);
    }

    void drawQuizzes() {
        float x = contentX();
        float w = min(900.f, contentW());

        titleField.rect = makeRect(x, 190, 260, 44);
        categoryField.rect = makeRect(x + 280, 190, 200, 44);
        themeField.rect = makeRect(x + 500, 190, 200, 44);
        bonusField.rect = makeRect(x, 250, 450, 44);

        drawText("Мои квизы", x, 126, 31, text);
        drawText("Создание и выбор квиза.", x, 166, 16, muted);

        drawInput(titleField);
        drawInput(categoryField);
        drawInput(themeField);
        drawInput(bonusField);
        addButton("addQuiz", "Создать квиз", x + w - 150, 190, 150, 44);

        float y = 330;
        auto list = userQuizzes();

        for (auto& q : list) {
            sf::Color quizColor = q.id == selectedQuizId ? violetSoft : card;
            sf::Color quizBorder = q.id == selectedQuizId ? accent : line;
            int leadCount = leadCountForQuiz(q.id);
            int questionCount = questionCountForQuiz(q.id);

            drawSurface(makeRect(x, y, w, 66), 20, quizColor, quizBorder, true);
            drawText(q.title + " / " + q.category, x + 22, y + 12, 18, text);
            drawText(
                "тема: " + q.theme + "   вопросов: " + to_string(questionCount) +
                    "   открытий: " + to_string(q.opens) +
                    "   заявок: " + to_string(leadCount),
                x + 22,
                y + 40,
                14,
                muted
            );
            addButton("selectQuiz_" + to_string(q.id), "Открыть", x + w - 210, y + 16, 90, 34, violet);
            addButton("deleteQuiz_" + to_string(q.id), "Удалить", x + w - 108, y + 16, 88, 34, danger);

            y += 78;
            if (y > footerY() - 70) {
                break;
            }
        }

        if (list.empty()) {
            drawSurface(makeRect(x, 330, w, 86), 22, card, sf::Color::Transparent, true);
            drawText("Квизов пока нет.", x + 24, 354, 18, text);
            drawText("Заполните поля выше или добавьте демо-квиз на главной странице.", x + 24, 384, 15, muted);
        }
    }

    void drawBuilder() {
        float x = contentX();
        float w = min(900.f, contentW());

        drawText("Конструктор вопросов", x, 126, 31, text);
        drawText("Выбранный квиз: " + selectedQuizStatus(), x, 166, 16, muted);

        if (!quizExists(selectedQuizId)) {
            drawSurface(makeRect(x, 220, w, 96), 22, card, sf::Color::Transparent, true);
            drawText("Сначала выберите квиз.", x + 24, 246, 19, text);
            drawText("Откройте раздел 'Квизы' и выберите запись.", x + 24, 278, 15, muted);
            return;
        }

        auto quizQuestionList = quizQuestions(selectedQuizId);
        float y = 206;

        drawText("Вопросы квиза", x, y, 20, text);
        y += 34;

        for (auto& q : quizQuestionList) {
            string answerLine;

            for (auto& a : questionAnswers(q.id)) {
                answerLine += a.text + " (" + to_string(a.score) + ")  ";
            }

            drawSurface(makeRect(x, y, w, 72), 18, q.id == editingQuestionId ? violetSoft : card, line, true);
            string meta = q.type;
            if (q.timeLimit > 0) {
                meta += " | " + to_string(q.timeLimit) + " сек";
            }
            if (!q.imagePath.empty()) {
                meta += " | картинка";
            }
            drawText(q.text + " / " + meta, x + 20, y + 11, 17, text);
            drawText(answerLine, x + 20, y + 41, 13, muted);
            addButton("editQuestion_" + to_string(q.id), "Изменить", x + w - 205, y + 19, 96, 34, violet);
            addButton("deleteQuestion_" + to_string(q.id), "Удалить", x + w - 98, y + 19, 86, 34, danger);

            y += 84;
            if (y > 430) {
                break;
            }
        }

        if (quizQuestionList.empty()) {
            drawSurface(makeRect(x, y, w, 76), 18, card, line, true);
            drawText("Вопросов пока нет.", x + 22, y + 18, 18, text);
            drawText("Добавьте первый вопрос в форме ниже.", x + 22, y + 48, 15, muted);
            y += 92;
        }

        float formTop = max(454.f, y + 16.f);
        drawText(editingQuestionId == -1 ? "Новый вопрос" : "Изменение вопроса", x, formTop, 20, text);

        questionField.rect = makeRect(x, formTop + 34, 440, 42);
        imagePathField.rect = makeRect(x + 462, formTop + 34, 290, 42);
        timeLimitField.rect = makeRect(x + 770, formTop + 34, 86, 42);

        drawInput(questionField);
        drawInput(imagePathField);
        drawInput(timeLimitField);

        float controlsY = formTop + 90;
        addNavButton("typeVariants", "Тест", x, controlsY, 86, 36, questionType == "варианты");
        addNavButton("typeText", "Текст", x + 96, controlsY, 88, 36, questionType == "текст");
        addNavButton("typeMatch", "Связи", x + 194, controlsY, 90, 36, questionType == "соответствие");
        addButton("answerMinus", "-", x + 310, controlsY, 38, 36, violet);
        drawText("ответов: " + to_string(answerCount), x + 358, controlsY + 8, 15, muted);
        addButton("answerPlus", "+", x + 456, controlsY, 38, 36, violet);
        addButton("addQuestion", editingQuestionId == -1 ? "Добавить вопрос" : "Сохранить вопрос", x + 520, controlsY, 178, 36);
        if (editingQuestionId != -1) {
            addButton("cancelQuestionEdit", "Отмена", x + 710, controlsY, 104, 36, danger);
        }

        float formY = controlsY + 58;
        int columns = answerCount > 5 ? 2 : 1;
        int rowsPerColumn = columns == 2 ? 5 : answerCount;
        float colW = columns == 2 ? 430.f : 500.f;

        for (int i = 0; i < answerCount; i++) {
            int col = i / rowsPerColumn;
            int row = i % rowsPerColumn;
            float ax = x + col * colW;
            float ay = formY + row * 54;
            string prefix = questionType == "соответствие" ? "пара " : (questionType == "текст" ? "правильный ответ " : "ответ ");

            answerFields[i].hint = prefix + to_string(i + 1);
            answerFields[i].rect = makeRect(ax, ay, 282, 42);
            scoreFields[i].rect = makeRect(ax + 296, ay, 82, 42);
            drawInput(answerFields[i]);
            drawInput(scoreFields[i]);
        }
    }

    void drawQuizStart() {
        float x = contentX();
        float w = min(900.f, contentW());
        auto list = userQuizzes();

        drawText("Выберите квиз", x, 126, 31, text);
        drawText("Выберите нужный квиз.", x, 166, 16, muted);

        float y = 220;
        for (auto& q : list) {
            drawSurface(makeRect(x, y, w, 92), 22, card, line, true);
            drawText(q.title, x + 24, y + 18, 22, text);
            drawText(q.category + " | " + q.theme + " | вопросов: " + to_string(questionCountForQuiz(q.id)), x + 24, y + 54, 15, muted);
            addButton("startQuiz_" + to_string(q.id), "Пройти", x + w - 130, y + 24, 106, 44, violet);
            y += 108;
            if (y > footerY() - 100) {
                break;
            }
        }

        if (list.empty()) {
            drawSurface(makeRect(x, 220, w, 86), 22, card, line, true);
            drawText("Нет доступных квизов.", x + 24, 244, 18, text);
            drawText("Добавьте квиз в разделе 'Квизы'.", x + 24, 274, 15, muted);
        }
    }

    void drawPreview() {
        float x = contentX();
        float w = min(860.f, contentW());

        drawText("Прохождение квиза", x, 126, 31, text);
        drawText(quizTitle(selectedQuizId), x, 166, 16, muted);

        auto quizQuestionList = quizQuestions(selectedQuizId);

        if (!quizQuestionList.empty() && previewIndex >= quizQuestionList.size()) {
            contactStep = true;
        }

        if (contactStep) {
            drawSurface(makeRect(x, 220, w, 180), 26, card, sf::Color::Transparent, true);
            drawText("Ваш результат: " + to_string(previewScore) + " баллов", x + 32, 260, 26, text);
            drawText("Оставьте контакт для сохранения результата.", x + 32, 304, 16, muted);

            leadNameField.rect = makeRect(x, 440, 270, 44);
            leadPhoneField.rect = makeRect(x + 292, 440, 270, 44);
            drawInput(leadNameField);
            drawInput(leadPhoneField);
            addButton("saveLead", "Сохранить заявку", x + 588, 440, 178, 44);
            return;
        }

        if (quizQuestionList.empty()) {
            drawText("Нет вопросов для прохождения.", x, 220, 17, muted);
            return;
        }

        auto question = quizQuestionList[previewIndex];
        drawSurface(makeRect(x, 220, w, 160), 26, card, sf::Color::Transparent, true);
        string questionMeta = question.type;
        if (question.timeLimit > 0) {
            questionMeta += " | лимит: " + to_string(question.timeLimit) + " сек";
        }
        if (!question.imagePath.empty()) {
            questionMeta += " | картинка: " + question.imagePath;
        }
        drawText(
            "Вопрос " + to_string(previewIndex + 1) + " из " + to_string(quizQuestionList.size()),
            x + 32,
            250,
            15,
            muted
        );
        drawText(questionMeta, x + 32, 274, 14, muted);
        drawText(question.text, x + 32, 304, 24, text);

        float y = 420;
        if (!question.imagePath.empty() && drawImage(question.imagePath, makeRect(x + 560, 244, 250, 118))) {
            drawText("Изображение", x + 560, 370, 13, muted);
        }

        if (question.type == "текст") {
            previewTextField.rect = makeRect(x, y, 360, 44);
            drawInput(previewTextField);
            addButton("submitTextAnswer", "Ответить", x + 382, y, 130, 44, violet);
        } else {
            for (auto& a : questionAnswers(question.id)) {
                addButton("answer_" + to_string(a.id), a.text, x, y, 430, 44, violet);
                y += 62;
            }
        }
    }

    bool drawImage(const string& path, sf::FloatRect area) {
        sf::Texture texture;
        if (!texture.loadFromFile(path)) {
            drawSurface(area, 18, cardSoft, line, false);
            drawCentered("картинка не найдена", area, 14, muted);
            return false;
        }

        sf::Sprite sprite(texture);
        auto size = texture.getSize();
        if (size.x == 0 || size.y == 0) {
            return false;
        }

        float scale = min(area.size.x / (float)size.x, area.size.y / (float)size.y);
        sprite.setScale({scale, scale});
        sprite.setPosition({
            snap(area.position.x + (area.size.x - size.x * scale) / 2.f),
            snap(area.position.y + (area.size.y - size.y * scale) / 2.f)
        });
        window.draw(sprite);
        return true;
    }

    void drawLeads() {
        float x = contentX();
        float w = min(900.f, contentW());

        searchField.rect = makeRect(x, 190, 390, 44);
        drawText("Заявки", x, 126, 31, text);
        drawText("Поиск по имени, телефону и названию квиза.", x, 166, 16, muted);
        drawInput(searchField);

        string search = lowerText(toUtf8(searchField.text));
        float y = 270;
        int shown = 0;

        for (auto& l : leads) {
            if (!quizExists(l.quizId)) {
                continue;
            }

            string rowText = lowerText(l.name + l.phone + quizTitle(l.quizId));

            if (!search.empty() && rowText.find(search) == string::npos) {
                continue;
            }

            drawSurface(makeRect(x, y, w, 66), 20, card, sf::Color::Transparent, true);
            drawText(l.name + "  " + l.phone, x + 22, y + 12, 18, text);
            drawText(
                quizTitle(l.quizId) + " | баллы: " + to_string(l.score) + " | " + l.date,
                x + 22,
                y + 40,
                14,
                muted
            );
            addButton("leadCert_" + to_string(l.id), "Сертификат", x + w - 232, y + 16, 120, 34, violet);
            addButton("deleteLead_" + to_string(l.id), "Удалить", x + w - 100, y + 16, 88, 34, danger);

            y += 78;
            shown++;

            if (y > footerY() - 70) {
                break;
            }
        }

        if (!shown) {
            drawText("Заявки не найдены.", x + 16, 270, 17, muted);
        }
    }

    void drawReports() {
        float x = contentX();

        drawText("Отчеты", x, 126, 31, text);
        drawText("Выбранный квиз: " + selectedQuizStatus(), x, 168, 16, muted);
        drawText("Папка отчета: data/reports.", x, 208, 16, muted);

        if (!quizExists(selectedQuizId)) {
            drawText("Выберите квиз в разделе 'Квизы'.", x, 250, 16, muted);
        }

        addButton("report", "Сформировать отчет", x, 286, 230, 44);
    }

    void drawHelp() {
        float x = contentX();
        vector<string> lines = {
            "1. Зарегистрируйтесь или войдите.",
            "2. Создайте квиз: название, категория, тема, бонус.",
            "3. Добавьте вопросы и ответы.",
            "4. Запустите прохождение квиза.",
            "5. После прохождения сохраните заявку.",
            "6. По заявке можно создать сертификат.",
            "7. В отчетах отображается статистика."
        };

        drawText("Справка", x, 126, 31, text);

        float y = 190;
        for (auto& line : lines) {
            drawText(line, x + 16, y, 17, text);
            y += 38;
        }
    }

    void drawMessage() {
        if (screen == Screen::Login) {
            return;
        }

        if (message.empty()) {
            lastMessage.clear();
            return;
        }

        if (message != lastMessage) {
            lastMessage = message;
            messageChangedAt = chrono::steady_clock::now();
        }

        auto now = chrono::steady_clock::now();
        auto seconds = chrono::duration_cast<chrono::seconds>(now - messageChangedAt).count();

        if (seconds >= 3) {
            message.clear();
            lastMessage.clear();
            return;
        }

        float x = 64.f;
        float w = contentW();

        drawSurface(makeRect(x, footerY(), w, 38), 12, sidebar, line, true);
        drawText(message, x + 18, footerY() + 10, 15, muted);
    }

    void drawPanel(float x, float y, float w, float h, const string& title, const string& body) {
        drawSurface(makeRect(x, y, w, h), 24, card, sf::Color::Transparent, true);
        drawRoundedRect(makeRect(x + 24, y + 24, 48, 8), 4, accent);
        drawText(title, x + 24, y + 42, 22, text);
        drawText(body, x + 24, y + 74, 16, muted);
    }

    void drawInput(InputField& f, bool password = false) {
        fields.push_back(&f);

        sf::Color border = f.active ? accent : line;
        sf::Color background = f.active ? fieldActive : field;

        drawSurface(f.rect, 16, background, border, false);

        string value = toUtf8(f.text);
        if (password) {
            value = string(value.size(), '*');
        }

        if (value.empty()) {
            value = f.hint;
        }

        sf::Color color = f.text.empty() ? sf::Color(112, 119, 113) : text;
        drawText(value, f.rect.position.x + 14, f.rect.position.y + 11, 16, color);
    }

    void addButton(const string& id,
                   const string& label,
                   float x,
                   float y,
                   float w,
                   float h,
                   sf::Color color = sf::Color(165, 128, 82)) {
        Button button{makeRect(x, y, w, h), label, id, color};
        buttons.push_back(button);

        if (color == sf::Color::Transparent) {
            return;
        }

        sf::Color buttonColor = hovered(button.rect) ? mix(color, 10) : color;
        sf::Color labelColor = (color == accent || color == violet) ? bg : sf::Color::White;

        drawSurface(button.rect, 6, buttonColor, sf::Color::Transparent, true);

        if (!label.empty()) {
            drawCentered(label, button.rect, 16, labelColor);
        }
    }

    void addNavButton(const string& id,
                      const string& label,
                      float x,
                      float y,
                      float w,
                      float h,
                      bool active) {
        sf::Color buttonColor = active ? accent : sf::Color(20, 25, 26);
        Button button{makeRect(x, y, w, h), label, id, buttonColor};
        buttons.push_back(button);

        if (hovered(button.rect)) {
            buttonColor = active ? mix(accent, 8) : sf::Color(30, 37, 37);
        }

        drawSurface(button.rect, 5, buttonColor, active ? accent : line, false);
        drawCentered(label, button.rect, 14, active ? bg : text);
    }

    void drawSurface(sf::FloatRect r,
                     float radius,
                     sf::Color fill,
                     sf::Color border = sf::Color::Transparent,
                     bool shadow = true) {
        radius = min(radius, 8.f);

        if (shadow) {
            drawRoundedRect(
                makeRect(r.position.x, r.position.y + 5, r.size.x, r.size.y),
                radius,
                sf::Color(0, 0, 0, 70)
            );
            drawRoundedRect(
                makeRect(r.position.x, r.position.y + 2, r.size.x, r.size.y),
                radius,
                sf::Color(0, 0, 0, 38)
            );
        }

        drawRoundedRect(r, radius, fill, border);
    }

    void drawRoundedRect(sf::FloatRect r,
                         float radius,
                         sf::Color fill,
                         sf::Color border = sf::Color::Transparent) {
        if (border != sf::Color::Transparent) {
            drawRoundedFill(
                makeRect(r.position.x - 1, r.position.y - 1, r.size.x + 2, r.size.y + 2),
                radius + 1,
                border
            );
        }

        drawRoundedFill(r, radius, fill);
    }

    void drawRoundedFill(sf::FloatRect r, float radius, sf::Color fill) {
        drawRect(makeRect(r.position.x + radius, r.position.y, r.size.x - radius * 2, r.size.y), fill);
        drawRect(makeRect(r.position.x, r.position.y + radius, r.size.x, r.size.y - radius * 2), fill);

        circle(r.position.x + radius, r.position.y + radius, radius, fill);
        circle(r.position.x + r.size.x - radius, r.position.y + radius, radius, fill);
        circle(r.position.x + radius, r.position.y + r.size.y - radius, radius, fill);
        circle(r.position.x + r.size.x - radius, r.position.y + r.size.y - radius, radius, fill);
    }

    void circle(float x, float y, float radius, sf::Color fill) {
        sf::CircleShape circle(radius);
        circle.setPointCount(48);
        circle.setFillColor(fill);
        circle.setPosition({snap(x - radius), snap(y - radius)});
        window.draw(circle);
    }

    void drawRect(sf::FloatRect r, sf::Color fill, sf::Color border = sf::Color::Transparent) {
        sf::RectangleShape shape({r.size.x, r.size.y});
        shape.setPosition({snap(r.position.x), snap(r.position.y)});
        shape.setFillColor(fill);
        shape.setOutlineColor(border);
        shape.setOutlineThickness(border == sf::Color::Transparent ? 0 : 1);
        window.draw(shape);
    }

    void drawCentered(const string& s, sf::FloatRect r, unsigned size, sf::Color color) {
        unsigned currentSize = size;
        sf::Text textObject(font, sfText(s), currentSize);
        textObject.setFillColor(color);

        auto bounds = textObject.getLocalBounds();
        while (bounds.size.x > r.size.x - 20 && currentSize > 11) {
            currentSize--;
            textObject.setCharacterSize(currentSize);
            bounds = textObject.getLocalBounds();
        }

        textObject.setPosition({
            snap(r.position.x + (r.size.x - bounds.size.x) / 2 - bounds.position.x),
            snap(r.position.y + (r.size.y - bounds.size.y) / 2 - bounds.position.y - 1)
        });

        window.draw(textObject);
    }

    void drawText(const string& s, float x, float y, unsigned size, sf::Color color) {
        sf::Text textObject(font, sfText(s), size);
        textObject.setFillColor(color);
        textObject.setPosition({snap(x), snap(y)});
        window.draw(textObject);
    }
};

int main() {
    MarquizApp app;
    app.run();
    return 0;
}
