/*
 * Copyright 2022 Rive
 */

#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "utils/no_op_factory.hpp"

class JSoner
{
    std::vector<bool> m_IsArray;

    void tab()
    {
        for (int i = 0; i < m_IsArray.size(); ++i)
        {
            printf("\t");
        }
    }
    void add_c(const char key[], char c)
    {
        this->tab();
        if (key)
        {
            printf("\"%s\": %c\n", key, c);
        }
        else
        {
            printf("%c\n", c);
        }
    }

public:
    JSoner() {}
    ~JSoner()
    {
        while (!m_IsArray.empty())
        {
            this->pop();
        }
    }

    void add(const char key[], const char value[])
    {
        this->tab();
        printf("\"%s\": \"%s\"\n", key, value);
    }
    void pushArray(const char key[] = nullptr)
    {
        this->add_c(key, '[');
        m_IsArray.push_back(true);
    }
    void pushStruct(const char key[] = nullptr)
    {
        this->add_c(key, '{');
        m_IsArray.push_back(false);
    }
    void pop()
    {
        assert(!m_IsArray.empty());
        char c = m_IsArray.front() ? ']' : '}';
        m_IsArray.pop_back();

        this->tab();
        printf("%c\n", c);
    }

    void add(const char key[], int value) { this->add(key, std::to_string(value).c_str()); }
};

//////////////////////////////////////////////////

static void dump(JSoner& js, rive::LinearAnimationInstance* anim)
{
    js.pushStruct();
    js.add("name", anim->name().c_str());
    js.add("duration", std::to_string(anim->durationSeconds()).c_str());
    js.add("loop", std::to_string(anim->loopValue()).c_str());
    js.pop();
}

static void dump(JSoner& js, rive::StateMachineInstance* smi)
{
    js.pushStruct();
    js.add("name", smi->name().c_str());
    if (auto count = smi->inputCount())
    {
        js.pushArray("inputs");
        for (auto i = 0; i < count; ++i)
        {
            auto inp = smi->input(i);
            js.add("name", inp->name().c_str());
        }
        js.pop();
    }
    js.pop();
}

static void dump(JSoner& js, rive::ArtboardInstance* abi)
{
    js.pushStruct();
    js.add("name", abi->name().c_str());
    if (auto count = abi->animationCount())
    {
        js.pushArray("animations");
        for (size_t i = 0; i < count; ++i)
        {
            dump(js, abi->animationAt(i).get());
        }
        js.pop();
    }
    if (auto count = abi->stateMachineCount())
    {
        js.pushArray("machines");
        for (size_t i = 0; i < count; ++i)
        {
            dump(js, abi->stateMachineAt(i).get());
        }
        js.pop();
    }
    js.pop();
}

static void dump(JSoner& js, rive::File* file)
{
    auto count = file->artboardCount();
    js.pushArray("artboards");
    for (size_t i = 0; i < count; ++i)
    {
        dump(js, file->artboardAt(i).get());
    }
    js.pop();
}

static std::unique_ptr<rive::File> open_file(const char name[])
{
    FILE* f = fopen(name, "rb");
    if (!f)
    {
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    auto length = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<uint8_t> bytes(length);

    if (fread(bytes.data(), 1, length, f) != length)
    {
        printf("Failed to read file into bytes array\n");
        return nullptr;
    }

    static rive::NoOpFactory gFactory;
    return rive::File::import(bytes, &gFactory);
}

static bool is_arg(const char arg[], const char target[], const char alt[] = nullptr)
{
    return !strcmp(arg, target) || (arg && !strcmp(arg, alt));
}

int main(int argc, const char* argv[])
{
    const char* filename = nullptr;

    for (int i = 1; i < argc; ++i)
    {
        if (is_arg(argv[i], "--file", "-f"))
        {
            filename = argv[++i];
            continue;
        }
        printf("Unrecognized argument %s\n", argv[i]);
        return 1;
    }

    if (!filename)
    {
        printf("Need --file filename\n");
        return 1;
    }

    auto file = open_file(filename);
    if (!file)
    {
        printf("Can't open %s\n", filename);
        return 1;
    }

    JSoner js;
    js.pushStruct();
    dump(js, file.get());
    return 0;
}
