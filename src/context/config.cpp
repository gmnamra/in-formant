#include "contextmanager.h"
#include "config.h"
#include "cfgpath.h"

using namespace Main;

static void initSubTable(toml::table &tbl, const std::string &name)
{
    if (!tbl[name]) {
        tbl.insert(name, toml::table());
    }
    else if (tbl[name].type() != toml::node_type::table) {
        tbl.insert_or_assign(name, toml::table());
    }
}

template<typename E>
constexpr auto enumInt(const E &e) noexcept {
    return static_cast<std::underlying_type_t<E>>(e);
}

template<typename T, typename V, typename ViewedType>
constexpr T valueCastField(toml::node_view<ViewedType> tblNode, const std::string &name, T defVal) {
    auto node = tblNode[name];
    auto opt = node.template value<V>();
    if (opt) {
        return static_cast<T>(opt.value());
    }
    else {
        tblNode.as_table()->insert(name, static_cast<V>(defVal));
        return defVal;
    }
}

template<typename T, typename ViewedType>
constexpr T valueField(toml::node_view<ViewedType> tblNode, const std::string &name, T defVal) {
    auto node = tblNode[name];
    auto opt = node.template value<T>();
    if (opt) {
        return static_cast<T>(opt.value());
    }
    else {
        tblNode.as_table()->insert(name, static_cast<T>(defVal));
        return defVal;
    }
}

template<typename T, typename ViewedType>
constexpr T enumField(toml::node_view<ViewedType> tblNode, const std::string &name, T defVal) {
    return valueCastField<T, int64_t, ViewedType>(tblNode, name, defVal);
}

template<typename ViewedType>
constexpr int64_t integerField(toml::node_view<ViewedType> tblNode, const std::string &name, int64_t defVal) {
    return valueField<int64_t, ViewedType>(tblNode, name, defVal);
}

template<typename ViewedType>
constexpr double doubleField(toml::node_view<ViewedType> tblNode, const std::string &name, double defVal) {
    return valueField<double, ViewedType>(tblNode, name, defVal);
}

template<typename ViewedType>
constexpr bool boolField(toml::node_view<ViewedType> tblNode, const std::string &name, bool defVal) {
    return valueField<bool, ViewedType>(tblNode, name, defVal);
}

fs::path Main::getConfigPath()
{
    char cfgdir[MAX_PATH];
    get_user_config_file(cfgdir, sizeof(cfgdir), "informant");
    if (cfgdir[0] == 0) {
        throw std::runtime_error("ContextManager] Unable to find config path.");
    }
    return fs::path(cfgdir);
}

toml::table Main::getConfigTable()
{
    auto path = getConfigPath();

    fs::create_directories(path.parent_path());

    if (fs::exists(path) && fs::is_regular_file(path)) {
        return toml::parse_file(path.string());
    }

    return toml::table();
}

Config::Config()
    : mTbl(getConfigTable())
{
    initSubTable(mTbl, "solvers");
    initSubTable(mTbl, "view");
    initSubTable(mTbl, "analysis");
}

Config::~Config()
{
    std::ofstream stream(getConfigPath());
    stream << mTbl;
}

Module::Audio::Backend Config::getAudioBackend()
{
    return enumField(toml::node_view(mTbl), "audioBackend", Main::getDefaultAudioBackend());
}

PitchAlgorithm Config::getPitchAlgorithm()
{
    return enumField(mTbl["solvers"], "pitch", PitchAlgorithm::RAPT);
}

LinpredAlgorithm Config::getLinpredAlgorithm()
{
    return enumField(mTbl["solvers"], "linpred", LinpredAlgorithm::Burg);
}

FormantAlgorithm Config::getFormantAlgorithm()
{
    return enumField(mTbl["solvers"], "formant", FormantAlgorithm::Filtered);
}

InvglotAlgorithm Config::getInvglotAlgorithm()
{
    return enumField(mTbl["solvers"], "invglot", InvglotAlgorithm::GFM_IAIF);
}

int Config::getViewFontSize()
{
#if defined(ANDROID) || defined(__ANDROID__)
    constexpr int defaultFontSize = 10;
#else
    constexpr int defaultFontSize = 14;
#endif

    return integerField(mTbl["view"], "fontSize", defaultFontSize);
}

int Config::getViewMinFrequency()
{
    return integerField(mTbl["view"], "minFrequency", 1);
}

int Config::getViewMaxFrequency()
{
    return integerField(mTbl["view"], "maxFrequency", 6000);
}

int Config::getViewFFTSize()
{
    return integerField(mTbl["view"], "fftSize", 2048);
}

int Config::getViewMinGain()
{
    return integerField(mTbl["view"], "minGain", -120);
}

int Config::getViewMaxGain()
{
    return integerField(mTbl["view"], "maxGain", 0);
}

FrequencyScale Config::getViewFrequencyScale()
{
    return enumField(mTbl["view"], "frequencyScale", FrequencyScale::Mel);
}

int Config::getViewFormantCount()
{
    return integerField(mTbl["view"], "formantCount", 4);
}

std::array<double, 3> Config::getViewFormantColor(int i)
{
    constexpr double defaultFormantColors[][3] = {
        {0.0,  1.0,  0.0},
        {0.57, 0.93, 0.57},
        {1.0,  0.0,  0.0},
    };

    const double defaultRed   = i < 3 ? defaultFormantColors[i][0] : 1.0;
    const double defaultGreen = i < 3 ? defaultFormantColors[i][1] : 0.6;
    const double defaultBlue  = i < 3 ? defaultFormantColors[i][2] : 1.0;

    auto formantColors = mTbl["view"]["formantColors"];
    
    if (!!formantColors && formantColors.is_array()) {
        auto& array = *formantColors.as_array();

        if (i < array.size() && array[i].is_array()) {
            auto& color = *array[i].as_array();
            color.resize(3, 1.0);

            auto colorIt = color.begin();
            double r, g, b;

            r = (colorIt++)->value_or(defaultRed);
            g = (colorIt++)->value_or(defaultGreen);
            b = (colorIt++)->value_or(defaultBlue);

            return {
                r, g, b
            };
        }
        else {
            array.resize(i + 1, toml::array{defaultRed, defaultGreen, defaultBlue});
        }
    }
    else {
        mTbl["view"].as_table()->insert_or_assign("formantColors", toml::array{
            toml::array{0.0,  1.0,  0.0},
            toml::array{0.57, 0.93, 0.57},
            toml::array{1.0,  0.0,  0.0},
        });
    }

    return getViewFormantColor(i);
}

bool Config::getViewShowSpectrogram()
{
    return boolField(mTbl["view"], "showSpectrogram", true);
}

void Config::setViewShowSpectrogram(bool b)
{
    mTbl["view"]["showSpectrogram"].ref<bool>() = b;
}

bool Config::getViewShowPitch()
{
    return boolField(mTbl["view"], "showPitch", false);
}

void Config::setViewShowPitch(bool b)
{
    mTbl["view"]["showPitch"].ref<bool>() = b;
}

bool Config::getViewShowFormants()
{
    return boolField(mTbl["view"], "showFormants", false);
}

void Config::setViewShowFormants(bool b)
{
    mTbl["view"]["showFormants"].ref<bool>() = b;
}

int Config::getAnalysisMaxFrequency()
{
    return integerField(mTbl["analysis"], "maxFrequency", 5200);
}

int Config::getAnalysisLpOffset()
{
    return integerField(mTbl["analysis"], "lpOffset", +1);
}

int Config::getAnalysisPitchSampleRate()
{
    return integerField(mTbl["analysis"], "pitchSampleRate", 32000);
}