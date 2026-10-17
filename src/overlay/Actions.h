#pragma once

namespace snapline {

enum class Action {
    Close,
    Copy,
    Save,
    Print,
    Upload,
    SelectFullScreen,
    Undo,
    Redo,
    ShareGoogle,
    ShareTinEye,
    ShareEmail,
    ShareTwitter,
    ShareFacebook,
    ShareVk,
    SharePinterest,
};

inline bool isShareAction(Action a)
{
    return a >= Action::ShareGoogle;
}

} // namespace snapline
