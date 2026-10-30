#pragma once

#include "Settings.h"
#include "TokenStore.h"
#include "YouTube/OAuthClient.h"
#include "YouTube/YouTubeClient.h"

namespace beatdrop
{

/** Connection state of the YouTube channel, shared by all plugin instances. */
class AuthController : public juce::ChangeBroadcaster
{
public:
    enum class State
    {
        signedOut,
        signingIn,
        signedIn
    };

    AuthController (OAuthClient&, YouTubeClient&);
    ~AuthController() override;

    State getState() const;
    juce::String getChannelTitle() const;
    juce::String getLastError() const;

    /** Fetches the channel name once, when a plugin window first needs it. */
    void loadChannelIfNeeded();

    void signIn();
    void cancelSignIn();
    void signOut();

private:
    void runInBackground (std::function<void()> job);
    void loadChannel();

    OAuthClient& oauth;
    YouTubeClient& youtube;

    mutable juce::CriticalSection lock;
    State state = State::signedOut;
    juce::String channelTitle, lastError;

    bool storedSessionChecked = false; // message thread only
    std::atomic<bool> abortSignIn { false };
    std::unique_ptr<juce::Thread> worker;
};

/** One instance per process, shared through juce::SharedResourcePointer. */
struct AppServices
{
    AppServices();

    Settings settings;
    TokenStore tokenStore;
    OAuthClient oauth { settings, tokenStore };
    YouTubeClient youtube;
    AuthController auth { oauth, youtube };
};

} // namespace beatdrop
