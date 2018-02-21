#include "SimHUD.h"
#include "ConstructorHelpers.h"
#include "Multirotor/SimModeWorldMultiRotor.h"
#include "Car/SimModeCar.h"
#include "controllers/Settings.hpp"
#include "MessageDialog.h"
#include "Kismet/KismetSystemLibrary.h"

#include <stdexcept>

ASimHUD* ASimHUD::instance_ = nullptr;

ASimHUD::ASimHUD()
{
    static ConstructorHelpers::FClassFinder<UUserWidget> hud_widget_class(TEXT("WidgetBlueprint'/AirSim/Blueprints/BP_SimHUDWidget'"));
    widget_class_ = hud_widget_class.Succeeded() ? hud_widget_class.Class : nullptr;
    instance_ = this;
}

void ASimHUD::BeginPlay()
{
    Super::BeginPlay();

    initializeSettings();

    //TODO: should we only do below on SceneCapture2D components and cameras?
    //avoid motion blur so capture images don't get
    GetWorld()->GetGameViewport()->GetEngineShowFlags()->SetMotionBlur(false);

    //use two different methods to set console var because sometime it doesn't seem to work
    static const auto custom_depth_var = IConsoleManager::Get().FindConsoleVariable(TEXT("r.CustomDepth"));
    custom_depth_var->Set(3);
    //Equivalent to enabling Custom Stencil in Project > Settings > Rendering > Postprocessing
    UKismetSystemLibrary::ExecuteConsoleCommand(GetWorld(), FString("r.CustomDepth 3"));

    //create main widget
    if (widget_class_ != nullptr) {
        widget_ = CreateWidget<USimHUDWidget>(this->GetOwningPlayerController(), widget_class_);
    }
    else {
        widget_ = nullptr;
        UAirBlueprintLib::LogMessage(TEXT("Cannot instantiate BP_SimHUDWidget blueprint!"), TEXT(""), LogDebugLevel::Failure, 180);
    }

    createSimMode();

    initializeSubWindows();

    setupInputBindings();

    widget_->AddToViewport();

    //synchronize PIP views
    widget_->initializeForPlay();
    widget_->setReportVisible(simmode_->EnableReport);
    widget_->setOnToggleRecordingHandler(std::bind(&ASimHUD::toggleRecordHandler, this));
    widget_->setRecordButtonVisibility(simmode_->isRecordUIVisible());
    updateWidgetSubwindowVisibility();
}

void ASimHUD::Tick(float DeltaSeconds)
{
    if (simmode_->EnableReport)
        widget_->updateReport(simmode_->getReport());
}

void ASimHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (widget_) {
        widget_->Destruct();
        widget_ = nullptr;
    }
    if (simmode_) {
        simmode_->Destroy();
        simmode_ = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void ASimHUD::toggleRecordHandler()
{
    simmode_->toggleRecording();
}

void ASimHUD::inputEventToggleRecording()
{
    toggleRecordHandler();
}

void ASimHUD::inputEventToggleReport()
{
    simmode_->EnableReport = !simmode_->EnableReport;
    widget_->setReportVisible(simmode_->EnableReport);
}

void ASimHUD::inputEventToggleHelp()
{
    widget_->toggleHelpVisibility();
}

void ASimHUD::inputEventToggleTrace()
{
    simmode_->getFpvVehiclePawnWrapper()->toggleTrace();
}

ASimHUD::ImageType ASimHUD::getSubwindowCameraType(int window_index)
{
    return subwindow_camera_types_[window_index]; //TODO: index check
}
void ASimHUD::setSubwindowCameraType(int window_index, ImageType type)
{
    subwindow_camera_types_[window_index] = type;
    updateWidgetSubwindowVisibility();
}

APIPCamera* ASimHUD::getSubwindowCamera(int window_index)
{
    return subwindow_cameras_[window_index]; //TODO: index check
}
void ASimHUD::setSubwindowCamera(int window_index, APIPCamera* camera)
{
    subwindow_cameras_[window_index] = camera; //TODO: index check
    updateWidgetSubwindowVisibility();
}

bool ASimHUD::getSubwindowVisible(int window_index)
{
    return subwindow_visible_[window_index];
}

void ASimHUD::setSubwindowVisible(int window_index, bool is_visible)
{
    subwindow_visible_[window_index] = is_visible;
    updateWidgetSubwindowVisibility();
}

void ASimHUD::updateWidgetSubwindowVisibility()
{
    for (int window_index = 0; window_index < kSubwindowCount; ++window_index) {
        APIPCamera* camera = subwindow_cameras_[window_index];
        ImageType camera_type = subwindow_camera_types_[window_index];

        bool is_visible = subwindow_visible_[window_index] && camera != nullptr;

        if (camera != nullptr)
            camera->setCameraTypeEnabled(camera_type, is_visible);

        widget_->setSubwindowVisibility(window_index,
            is_visible,
            is_visible ? camera->getRenderTarget(camera_type, false) : nullptr
        );
    }
}

bool ASimHUD::isWidgetSubwindowVisible(int window_index)
{
    return widget_->getSubwindowVisibility(window_index) != 0;
}

void ASimHUD::inputEventToggleSubwindow0()
{
    subwindow_visible_[0] = !subwindow_visible_[0];
    updateWidgetSubwindowVisibility();
}

void ASimHUD::inputEventToggleSubwindow1()
{
    subwindow_visible_[1] = !subwindow_visible_[1];
    updateWidgetSubwindowVisibility();
}

void ASimHUD::inputEventToggleSubwindow2()
{
    subwindow_visible_[2] = !subwindow_visible_[2];
    updateWidgetSubwindowVisibility();
}

void ASimHUD::inputEventToggleAll()
{
    subwindow_visible_[0] = !subwindow_visible_[0];
    subwindow_visible_[1] = subwindow_visible_[2] = subwindow_visible_[0];
    updateWidgetSubwindowVisibility();
}


void ASimHUD::setupInputBindings()
{
    UAirBlueprintLib::EnableInput(this);

    UAirBlueprintLib::BindActionToKey("inputEventToggleRecording", EKeys::R, this, &ASimHUD::inputEventToggleRecording);
    UAirBlueprintLib::BindActionToKey("InputEventToggleReport", EKeys::Semicolon, this, &ASimHUD::inputEventToggleReport);
    UAirBlueprintLib::BindActionToKey("InputEventToggleHelp", EKeys::F1, this, &ASimHUD::inputEventToggleHelp);
    UAirBlueprintLib::BindActionToKey("InputEventToggleTrace", EKeys::T, this, &ASimHUD::inputEventToggleTrace);

    UAirBlueprintLib::BindActionToKey("InputEventToggleSubwindow0", EKeys::One, this, &ASimHUD::inputEventToggleSubwindow0);
    UAirBlueprintLib::BindActionToKey("InputEventToggleSubwindow1", EKeys::Two, this, &ASimHUD::inputEventToggleSubwindow1);
    UAirBlueprintLib::BindActionToKey("InputEventToggleSubwindow2", EKeys::Three, this, &ASimHUD::inputEventToggleSubwindow2);
    UAirBlueprintLib::BindActionToKey("InputEventToggleAll", EKeys::Zero, this, &ASimHUD::inputEventToggleAll);
}

void ASimHUD::createSimMode()
{
    Settings& settings = Settings::singleton();
    std::string simmode_name = settings.getString("SimMode", "");
    if (simmode_name == "") {
        FText title = FText::FromString("Choose Vehicle");
        if (EAppReturnType::No == FMessageDialog::Open(EAppMsgType::YesNo,
            FText::FromString("Would you like to use car simulation? Choose no to use quadrotor simulation."),
            &title))
        {
            simmode_name = "Multirotor";
        }
        else
            simmode_name = "Car";

        settings.setString("SimMode", simmode_name);
    }

    FActorSpawnParameters simmode_spawn_params;
    simmode_spawn_params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    if (simmode_name == "Multirotor")
        simmode_ = this->GetWorld()->SpawnActor<ASimModeWorldMultiRotor>(FVector::ZeroVector, FRotator::ZeroRotator, simmode_spawn_params);
    else if (simmode_name == "Car")
        simmode_ = this->GetWorld()->SpawnActor<ASimModeCar>(FVector::ZeroVector, FRotator::ZeroRotator, simmode_spawn_params);
    else
        UAirBlueprintLib::LogMessageString("SimMode is not valid: ", simmode_name, LogDebugLevel::Failure);
}


void ASimHUD::initializeSubWindows()
{
    auto wrapper = simmode_->getFpvVehiclePawnWrapper();
    auto camera_count = wrapper->getCameraCount();

    //setup defaults
    if (camera_count > 0) {
        subwindow_cameras_[0] = wrapper->getCamera(0);
        subwindow_cameras_[1] = wrapper->getCamera(0); //camera_count > 3 ? 3 : 0
        subwindow_cameras_[2] = wrapper->getCamera(0); //camera_count > 4 ? 4 : 0
    }
    else
        subwindow_cameras_[0] = subwindow_cameras_[1] = subwindow_cameras_[2] = nullptr;

    subwindow_camera_types_[0] = ImageType::DepthVis;
    subwindow_camera_types_[1] = ImageType::Segmentation;
    subwindow_camera_types_[2] = ImageType::Scene;
    subwindow_visible_[0] = subwindow_visible_[1] = subwindow_visible_[2] = false;

    Settings& json_settings_root = Settings::singleton();
    Settings json_settings_parent;
    if (json_settings_root.getChild("SubWindows", json_settings_parent)) {
        for (size_t child_index = 0; child_index < json_settings_parent.size(); ++child_index) {
            Settings json_settings_child;
            if (json_settings_parent.getChild(child_index, json_settings_child)) {
                int index = json_settings_child.getInt("WindowID", -1);

                if (index == -1) {
                    UAirBlueprintLib::LogMessageString("WindowID not set in <SubWindows> element(s) in settings.json",
                        std::to_string(child_index), LogDebugLevel::Failure);
                    continue;
                }

                subwindow_camera_types_[index] = Utils::toEnum<ImageType>(json_settings_child.getInt("ImageType", 0));
                subwindow_visible_[index] = json_settings_child.getBool("Visible", false);

                int camera_id = json_settings_child.getInt("CameraID", 0);
                if (camera_id >= 0 && camera_id < camera_count)
                    subwindow_cameras_[index] = wrapper->getCamera(camera_id);
                else

                    UAirBlueprintLib::LogMessageString("CameraID in <SubWindows> element in settings.json is invalid",
                        std::to_string(child_index), LogDebugLevel::Failure);
            }
        }
    }
}

void ASimHUD::initializeSettings()
{
    //TODO: should this be done somewhere else?
    //load settings file if found
    typedef msr::airlib::Settings Settings;
    try {
        std::string settings_str;
        std::string json_settings_text;
        bool load_success = false;
        bool settings_found = getSettingsText(json_settings_text);
        if (settings_found && json_settings_text.size() > 0) {
            Settings& settings = Settings::loadJSonString(json_settings_text);
            if (settings.isLoadSuccess()) {
                UAirBlueprintLib::setLogMessagesHidden(!settings.getBool("LogMessagesVisible", true));
                load_success = true;
            }
            else {
                UAirBlueprintLib::LogMessageString("Cannot parse JSON settings string.", "", LogDebugLevel::Failure);
            }
        }

        if (!load_success) {
            FString settings_filename = FString(Settings::getUserDirectoryFullPath("settings.json").c_str());
            //create default settings
            Settings& settings = Settings::loadJSonString("{}");
            //write some settings in new file otherwise the string "null" is written if all settigs are empty
            settings.setString("SeeDocsAt", "https://github.com/Microsoft/AirSim/blob/master/docs/settings.md");
            settings.setDouble("SettingsVersion", 1.0);

            if (!settings_found) {
                std::string json_content;
                //TODO: there is a crash in Linux due to settings.saveJSonString(). Remove this workaround after we only support Unreal 4.17
                //https://answers.unrealengine.com/questions/664905/unreal-crashes-on-two-lines-of-extremely-simple-st.html
#ifdef _WIN32
                json_content = settings.saveJSonString();
#else
                json_content = "{ \"SettingsVersion\": 1, \"SeeDocsAt\": \"https://github.com/Microsoft/AirSim/blob/master/docs/settings.md\"}";
#endif
                FString json_fstring = FString(json_content.c_str());
                FFileHelper::SaveStringToFile(json_fstring, *settings_filename);
                UAirBlueprintLib::LogMessageString("Settings not provided. Created defalut settings file at ", TCHAR_TO_UTF8(*settings_filename), LogDebugLevel::Informational);
            }
        }
    }
    catch (std::exception& ex) {
        UAirBlueprintLib::LogMessage(FString("Error loading settings"), FString(ex.what()), LogDebugLevel::Failure, 30);
    }
}

// Attempts to parse the settings text from one of multiple locations.
// First, check the command line for settings provided via "-s" or "--settings" arguments
// Next, check the executable's working directory for the settings file.
// Finally, check the user's documents folder. 
// If the settings file cannot be read, throw an exception

bool ASimHUD::getSettingsText(std::string& settingsText) {
    return (getSettingsTextFromCommandLine(settingsText)
        ||
        readSettingsTextFromFile(FString(Settings::getExecutableFullPath("settings.json").c_str()), settingsText)
        ||
        readSettingsTextFromFile(FString(Settings::getUserDirectoryFullPath("settings.json").c_str()), settingsText));
}

// Attempts to parse the settings text from the command line
// Looks for the flag "--settings". If it exists, settingsText will be set to the value.
// Example: AirSim.exe -s '{"foo" : "bar"}' -> settingsText will be set to {"foo": "bar"}
// Returns true if the argument is present, false otherwise.
bool ASimHUD::getSettingsTextFromCommandLine(std::string& settingsText) {

    bool found = false;
    FString settingsTextFString;
    const TCHAR* commandLineArgs = FCommandLine::Get();

    if (FParse::Param(commandLineArgs, TEXT("-settings"))) {
        FString commandLineArgsFString = FString(commandLineArgs);
        int idx = commandLineArgsFString.Find(TEXT("-settings"));
        FString settingsJsonFString = commandLineArgsFString.RightChop(idx + 10);
        if (FParse::QuotedString(*settingsJsonFString, settingsTextFString)) {
            settingsText = std::string(TCHAR_TO_UTF8(*settingsTextFString));
            found = true;
        }
    }

    return found;
}

bool ASimHUD::readSettingsTextFromFile(FString settingsFilepath, std::string& settingsText) {

    bool found = FPaths::FileExists(settingsFilepath);
    if (found) {
        FString settingsTextFStr;
        bool readSuccessful = FFileHelper::LoadFileToString(settingsTextFStr, *settingsFilepath);
        if (readSuccessful) {
            UAirBlueprintLib::LogMessageString("Loaded settings from ", TCHAR_TO_UTF8(*settingsFilepath), LogDebugLevel::Informational);
            settingsText = TCHAR_TO_UTF8(*settingsTextFStr);
        }
        else {
            UAirBlueprintLib::LogMessageString("Cannot read file ", TCHAR_TO_UTF8(*settingsFilepath), LogDebugLevel::Failure);
            throw std::runtime_error("Cannot read settings file.");
        }
    }

    return found;
}