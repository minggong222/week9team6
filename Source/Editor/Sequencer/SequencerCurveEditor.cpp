#include "EnginePCH.h"
#include "SequencerPanel.h"
#include "ThirdParty/ImCurveEdit/ImCurveEdit.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    float ReadChannel(const FSequenceKeyframe& Key, bool, int) { return Key.Value; }
    class FSequenceCurveAdapter : public ImCurveEdit::Delegate
    {
    public:
        FSequenceCurveAdapter(FSequenceSection& InSection, bool InCamera, int InChannel,
            float InFPS, float InDuration, const FSequenceCameraValue& InBase,
            ImVec2& InMin, ImVec2& InMax, std::function<void(float)> InChanged)
            : Section(InSection), Camera(InCamera), Channel(InChannel), FPS(InFPS), Duration(InDuration),
            Base(InBase), Min(InMin), Max(InMax), Changed(std::move(InChanged)) { Refresh(); }

        size_t GetCurveCount() override { return 1; }
        size_t GetPointCount(size_t) override { return Points.size(); }
        ImVec2* GetPoints(size_t) override { return Points.data(); }
        ImVec2& GetMin() override { return Min; }
        ImVec2& GetMax() override { return Max; }
        uint32_t GetCurveColor(size_t) override { return IM_COL32(235, 145, 75, 255); }
        ImCurveEdit::CurveType GetCurveType(size_t) const override
        {
            return CurveType(Mode());
        }
        ImCurveEdit::CurveType GetSegmentType(size_t, size_t PointIndex) const override
        {
            return CurveType(GetKeys()[PointIndex].Interpolation.value_or(Mode()));
        }
        static ImCurveEdit::CurveType CurveType(ESequenceInterpolation Mode)
        {
            if (Mode == ESequenceInterpolation::EaseIn) return ImCurveEdit::CurveEaseIn;
            if (Mode == ESequenceInterpolation::EaseOut) return ImCurveEdit::CurveEaseOut;
            if (Mode == ESequenceInterpolation::EaseInOut) return ImCurveEdit::CurveSmooth;
            if (Mode == ESequenceInterpolation::Constant) return ImCurveEdit::CurveDiscrete;
            return ImCurveEdit::CurveLinear;
        }

        int EditPoint(size_t, int Index, ImVec2 Point) override
        {
            const auto Keys = GetKeys();
            if (Index < 0 || Index >= static_cast<int>(Keys.size()) || !std::isfinite(Point.x) || !std::isfinite(Point.y)) return Index;
            // Keep indices stable during multi-selection drags; keys cannot cross their neighbours.
            float Low = 0, High = Duration;
            if (Index > 0) Low = std::max(Low, std::nextafter(Keys[Index - 1].Time, std::numeric_limits<float>::infinity()));
            if (Index + 1 < static_cast<int>(Keys.size())) High = std::min(High, std::nextafter(Keys[Index + 1].Time, -std::numeric_limits<float>::infinity()));
            if (Low > High) return Index;
            auto Key = Keys[Index];
            Key.Time = std::clamp(Point.x / FPS, Low, High);
            SetChannel(Key, Point.y);
            if (Camera)
                Section.CameraChannels[Channel].EditKey(Keys[Index].Time, Key.Time, Key.Value);
            else
            {
                FSequenceSection Updated;
                Updated.StartTime = Section.StartTime; Updated.EndTime = Section.EndTime; Updated.Interpolation = Section.Interpolation;
                for (int I = 0; I < static_cast<int>(Keys.size()); ++I) Insert(Updated, I == Index ? Key : Keys[I]);
                Section = std::move(Updated);
            }
            Refresh();
            bChanged = true;
            Changed(Key.Time);
            return Index;
        }

        void AddPoint(size_t, ImVec2 Point) override
        {
            if (!std::isfinite(Point.x) || !std::isfinite(Point.y)) return;
            FSequenceKeyframe Key;
            Key.Time = std::clamp(Point.x / FPS, 0.0f, Duration);
            Key.Camera = Base;

            SetChannel(Key, Point.y);
            Insert(Section, Key);
            Refresh();
            bChanged = true;
            Changed(Key.Time);
        }

        void Fit()
        {
            float Low = 0, High = 1;
            if (Camera)
            {
                FSequenceKeyframe Key; Key.Camera = Base;
                Low = High = Base.GetChannelValue(Channel);
                for (const auto& Point : Points) { Low = std::min(Low, Point.y); High = std::max(High, Point.y); }
            }
            const float Margin = std::max(0.1f, (High - Low) * 0.1f);
            Min = ImVec2(-Duration * FPS * 0.05f, Low - Margin);
            Max = ImVec2(Duration * FPS * 1.05f - 1.0f, High + Margin);
        }

        unsigned int GetBackgroundColor() override
        {
            Origin = ImGui::GetCursorScreenPos();
            ImDrawList* Draw = ImGui::GetWindowDrawList();
            Draw->AddRectFilled(Origin, ImVec2(Origin.x + Size.x, Origin.y + Size.y), IM_COL32(27, 28, 31, 255));
            const float SpanX = Max.x - Min.x + 1, SpanY = Max.y - Min.y;
            const auto Step = [](float Raw) {
                const float Magnitude = std::pow(10.0f, std::floor(std::log10(Raw)));
                const float N = Raw / Magnitude;
                return (N <= 1 ? 1 : N <= 2 ? 2 : N <= 5 ? 5 : 10) * Magnitude;
            };
            const float TimeStep = Step(SpanX / FPS * 90 / Size.x);
            const float ValueStep = Step(SpanY * 45 / Size.y);
            for (double T = std::ceil(Min.x / FPS / TimeStep) * TimeStep; T <= (Max.x + 1) / FPS; T += TimeStep)
            {
                const float X = Origin.x + (static_cast<float>(T) * FPS - Min.x) / SpanX * Size.x;
                Draw->AddLine(ImVec2(X, Origin.y), ImVec2(X, Origin.y + Size.y), IM_COL32(65, 65, 70, 150));
                char Label[32]; ImFormatString(Label, sizeof(Label), "%.2fs", T);
                Draw->AddText(ImVec2(X + 3, Origin.y + Size.y - ImGui::GetTextLineHeight()), IM_COL32(155, 155, 165, 255), Label);
            }
            for (double V = std::ceil(Min.y / ValueStep) * ValueStep; V <= Max.y; V += ValueStep)
            {
                const float Y = Origin.y + (Max.y - static_cast<float>(V)) / SpanY * Size.y;
                Draw->AddLine(ImVec2(Origin.x, Y), ImVec2(Origin.x + Size.x, Y), IM_COL32(65, 65, 70, 150));
                char Label[32]; ImFormatString(Label, sizeof(Label), "%.2f", V);
                Draw->AddText(ImVec2(Origin.x + 3, Y + 2), IM_COL32(155, 155, 165, 255), Label);
            }
            const float X = Origin.x + (CurrentFrame - Min.x) / SpanX * Size.x;
            Draw->AddLine(ImVec2(X, Origin.y), ImVec2(X, Origin.y + Size.y), IM_COL32(245, 80, 85, 200), 1.5f);
            return 0; // Keep this grid visible beneath ImCurveEdit's curves.
        }

        ImVec2 Origin{}, Size{};
        float CurrentFrame = 0;
        bool bChanged = false;

    private:
        void Refresh()
        {
            Points.resize(GetKeys().size());
            for (size_t I = 0; I < Points.size(); ++I)
            {
                float Value = ReadChannel(GetKeys()[I], Camera, Channel);
                if (Camera && Channel >= 3 && Channel < 6 && I > 0)
                    Value = Points[I - 1].y + std::remainder(Value - Points[I - 1].y, 360.0f);
                Points[I] = ImVec2(GetKeys()[I].Time * FPS, Value);
            }
        }
        void SetChannel(FSequenceKeyframe& Key, float Value) const
        {
            Key.Value = !Camera ? std::clamp(Value, 0.0f, 1.0f) : FSequenceCameraValue::ClampChannelValue(Channel, Value);
        }
        void Insert(FSequenceSection& Target, const FSequenceKeyframe& Key) const
        {
            if (Camera) Target.CameraChannels[Channel].AddKey(Key.Time, Key.Value, Key.Interpolation);
            else Target.AddKey(Key.Time, Key.Value, Key.Interpolation);
        }
        const std::vector<FSequenceKeyframe>& GetKeys() const { return Camera ? Section.CameraChannels[Channel].GetKeys() : Section.GetKeys(); }
        ESequenceInterpolation Mode() const { return Camera ? Section.CameraChannels[Channel].Interpolation : Section.Interpolation; }
        FSequenceSection& Section;
        bool Camera;
        int Channel;
        float FPS, Duration;
        FSequenceCameraValue Base;
        ImVec2& Min;
        ImVec2& Max;
        std::function<void(float)> Changed;
        std::vector<ImVec2> Points;
    };
}

void FSequencerPanel::DrawCurveEditor()
{
    ImGui::SetNextWindowSize(ImVec2(740, 380), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Curve Editor", &bCurveEditorOpen)) { ImGui::End(); return; }
    const int TrackIndex = GetSelectedTrackIndex();
    auto* Section = GetSelectedSection();
    if (!Section || TrackIndex < 0) { ImGui::TextDisabled("Select a track in Sequencer."); ImGui::End(); return; }
    const auto& Track = EditorTracks[TrackIndex];
    const bool Camera = Track.Type == 1;
    bool Fit = CurveTrackId != Track.Id;
    if (Fit) { CurveTrackId = Track.Id; if (!Camera) CurveChannel = 0; ++CurveEditorSerial; }
    ImGui::TextUnformatted(Track.Name.c_str());
    const float FPS = Player.GetFramesPerSecond();
    if (Camera)
    {
        ImGui::SameLine(); ImGui::SetNextItemWidth(150);
        if (ImGui::Combo("##Channel", &CurveChannel, "Location X\0Location Y\0Location Z\0Pitch\0Yaw\0Roll\0FOV\0Projection\0Ortho Width\0"))
        { SelectCameraChannel(CurveChannel); Fit = true; ++CurveEditorSerial; }
    }
    ImGui::SameLine(); ImGui::SetNextItemWidth(110);
    auto& Interpolation = Camera ? Section->CameraChannels[CurveChannel].Interpolation : Section->Interpolation;
    ImGui::TextUnformatted("Default");
    ImGui::SameLine();
    int Mode = static_cast<int>(Interpolation);
    if (ImGui::Combo("##Interpolation", &Mode, "Linear\0Ease In Out\0Step\0Ease In\0Ease Out\0"))
    { Interpolation = static_cast<ESequenceInterpolation>(Mode); NotifySequenceEdited(); }
    ImGui::SameLine(); if (ImGui::Button("Fit")) Fit = true;
    ImGui::SameLine(); if (ImGui::Button("+ Key")) { if (Camera) SelectCameraChannel(CurveChannel); Player.Pause(); AddKey(Player.GetCurrentTime()); }
    ImGui::SameLine(); ImGui::BeginDisabled(!SelectedKeyTime);
    if (ImGui::Button("Delete Key")) { const auto Time = SelectedKeyTime; if (Camera) SelectCameraChannel(CurveChannel); SelectedKeyTime = Time; DeleteSelectedKey(); }
    ImGui::EndDisabled();

    const auto Changed = [this, Camera](float Time) { if (Camera) SelectCameraChannel(CurveChannel); SelectedKeyTime = Time; Player.Pause(); NotifySequenceEdited(); };
    const auto Base = Camera ? GetCameraKeyValue(Player.GetCurrentTime()) : FSequenceCameraValue{};
    if (SelectedKeyTime)
    {
        const auto& Keys = (Camera ? Section->CameraChannels[CurveChannel].GetKeys() : Section->GetKeys());
        const auto Found = std::find_if(Keys.begin(), Keys.end(), [this](const auto& Key) { return Key.Time == *SelectedKeyTime; });
        if (Found != Keys.end())
        {
            const int Index = static_cast<int>(Found - Keys.begin());
            float Time = Found->Time, Value = ReadChannel(*Found, Camera, CurveChannel);
            ImGui::SetNextItemWidth(100);
            bool Edited = ImGui::DragFloat("Time (s)", &Time, 0.01f, 0, PreviewSequence->Duration, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine(); ImGui::SetNextItemWidth(100);
            Edited |= ImGui::DragFloat("Value", &Value, 0.01f, 0, 0, "%.3f");
            if (Edited)
            {
                FSequenceCurveAdapter Adapter(*Section, Camera, CurveChannel, FPS, PreviewSequence->Duration, Base, CurveMin, CurveMax, Changed);
                Adapter.EditPoint(0, Index, ImVec2(Time * FPS, Value));
            }
        }
    }
    DrawSelectedKeyInterpolation();
    ImGui::TextDisabled("Key -> next sets the outgoing segment. Parent rows include child keys at the same time.");
    ImGui::TextDisabled("Double-click: add key | Drag: edit | Wheel: value zoom | Shift+wheel: time zoom | Middle-drag: pan values");
    FSequenceCurveAdapter Adapter(*Section, Camera, CurveChannel, FPS, PreviewSequence->Duration, Base, CurveMin, CurveMax, Changed);
    if (Fit || CurveMax.y <= CurveMin.y || CurveMax.x - CurveMin.x + 1 <= 0) Adapter.Fit();
    const int Count = static_cast<int>((Camera ? Section->CameraChannels[CurveChannel].GetKeys() : Section->GetKeys()).size());
    if (CurvePointCount != Count) { CurvePointCount = Count; ++CurveEditorSerial; }
    Adapter.Size = ImVec2(std::max(80.0f, ImGui::GetContentRegionAvail().x), std::max(100.0f, ImGui::GetContentRegionAvail().y));
    Adapter.CurrentFrame = Player.GetCurrentTime() * FPS;
    auto& IO = ImGui::GetIO();
    const ImVec2 Start = ImGui::GetCursorScreenPos();
    const bool Hovered = ImRect(Start, ImVec2(Start.x + Adapter.Size.x, Start.y + Adapter.Size.y)).Contains(IO.MousePos);
    const float Wheel = IO.MouseWheel;
    if (Hovered && IO.KeyShift && Wheel != 0)
    {
        const float Ratio = std::clamp((IO.MousePos.x - Start.x) / Adapter.Size.x, 0.0f, 1.0f);
        const float Span = CurveMax.x - CurveMin.x + 1;
        const float Anchor = CurveMin.x + Ratio * Span;
        const float NewSpan = std::clamp(Span * std::pow(0.9f, Wheel), 1.0f, 10000000.0f);
        CurveMin.x = Anchor - Ratio * NewSpan; CurveMax.x = CurveMin.x + NewSpan - 1;
        IO.MouseWheel = 0;
    }
    ImVector<ImCurveEdit::EditPoint> Selection;
    ImGui::PushID(CurveEditorSerial);
    ImCurveEdit::Edit(Adapter, Adapter.Size, ImGui::GetID("Graph"), nullptr, &Selection);
    ImGui::PopID();
    IO.MouseWheel = Wheel;
    const ImRect Plot(Adapter.Origin, ImVec2(Adapter.Origin.x + Adapter.Size.x, Adapter.Origin.y + Adapter.Size.y));
    if (Plot.Contains(IO.MousePos) && ImGui::IsMouseDoubleClicked(0) && !Adapter.bChanged)
    {
        bool OnKey = false;
        for (size_t I = 0; I < Adapter.GetPointCount(0); ++I)
        {
            const auto Point = Adapter.GetPoints(0)[I];
            const float X = Adapter.Origin.x + (Point.x - CurveMin.x) / (CurveMax.x - CurveMin.x + 1) * Adapter.Size.x;
            const float Y = Adapter.Origin.y + (CurveMax.y - Point.y) / (CurveMax.y - CurveMin.y) * Adapter.Size.y;
            OnKey |= std::abs(X - IO.MousePos.x) < 8 && std::abs(Y - IO.MousePos.y) < 8;
        }
        if (!OnKey)
            Adapter.AddPoint(0, ImVec2(CurveMin.x + (IO.MousePos.x - Adapter.Origin.x) / Adapter.Size.x * (CurveMax.x - CurveMin.x + 1),
                CurveMax.y - (IO.MousePos.y - Adapter.Origin.y) / Adapter.Size.y * (CurveMax.y - CurveMin.y)));
    }
    if (!Selection.empty() && Plot.Contains(IO.MousePos) && ImGui::IsMouseClicked(0))
    {
        const int Index = Selection.back().pointIndex;
        if (Index >= 0 && Index < static_cast<int>((Camera ? Section->CameraChannels[CurveChannel].GetKeys() : Section->GetKeys()).size()))
        {
            if (Camera) SelectCameraChannel(CurveChannel);
            SelectedKeyTime = (Camera ? Section->CameraChannels[CurveChannel].GetKeys() : Section->GetKeys())[Index].Time;
        }
    }
    if (Adapter.focused && !IO.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete) && SelectedKeyTime)
    { const auto Time = SelectedKeyTime; if (Camera) SelectCameraChannel(CurveChannel); SelectedKeyTime = Time; DeleteSelectedKey(); }
    ImGui::End();
}
