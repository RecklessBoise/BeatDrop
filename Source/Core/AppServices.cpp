#include "AppServices.h"

namespace beatdrop
{

namespace
{
    class LambdaThread : public juce::Thread
    {
    public:
        explicit LambdaThread (std::function<void()> j)
            : juce::Thread ("BeatDrop account"), job (std::move (j)) {}

        void run() override   { job(); }

    private:
        std::function<void()> job;
    };
}

AuthController::AuthController (OAuthClient& o, YouTubeClient& y)
    : oauth (o), youtube (y)
{
    // No Keychain or network access here: hosts construct plugins while scanning.
}

void AuthController::loadChannelIfNeeded()
{
    if (! storedSessionChecked)
    {
        storedSessionChecked = true;

        if (oauth.isSignedIn())
        {
            {
                const juce::ScopedLock sl (lock);
                state = State::signedIn;
            }

            sendChangeMessage();
        }
    }

    if (getState() != State::signedIn || getChannelTitle().isNotEmpty())
        return;

    if (worker != nullptr && worker->isThreadRunning())
        return;

    runInBackground ([this] { loadChannel(); });
}

AuthController::~AuthController()
{
    abortSignIn = true;

    if (worker != nullptr)
        worker->stopThread (10000);
}

AuthController::State AuthController::getState() const
{
    const juce::ScopedLock sl (lock);
    return state;
}

juce::String AuthController::getChannelTitle() const
{
    const juce::ScopedLock sl (lock);
    return channelTitle;
}

juce::String AuthController::getLastError() const
{
    const juce::ScopedLock sl (lock);
    return lastError;
}

void AuthController::runInBackground (std::function<void()> job)
{
    if (worker != nullptr)
    {
        abortSignIn = true;
        worker->stopThread (5000);
    }

    abortSignIn = false;
    worker = std::make_unique<LambdaThread> (std::move (job));
    worker->startThread();
}

void AuthController::signIn()
{
    {
        const juce::ScopedLock sl (lock);
        state = State::signingIn;
        lastError.clear();
    }

    sendChangeMessage();

    runInBackground ([this]
    {
        const auto result = oauth.signIn ([this] { return abortSignIn.load(); });

        if (result.failed())
        {
            {
                const juce::ScopedLock sl (lock);
                state = State::signedOut;
                lastError = result.getErrorMessage();
            }

            sendChangeMessage();
            return;
        }

        {
            const juce::ScopedLock sl (lock);
            state = State::signedIn;
        }

        loadChannel();
    });
}

void AuthController::cancelSignIn()
{
    abortSignIn = true;
}

void AuthController::signOut()
{
    abortSignIn = true;
    oauth.signOut();

    {
        const juce::ScopedLock sl (lock);
        state = State::signedOut;
        channelTitle.clear();
        lastError.clear();
    }

    sendChangeMessage();
}

void AuthController::loadChannel()
{
    ChannelInfo channel;
    const auto result = youtube.fetchMyChannel (channel);

    {
        const juce::ScopedLock sl (lock);

        if (result.wasOk())
        {
            channelTitle = channel.title;
            lastError.clear();
        }
        else
        {
            lastError = result.getErrorMessage();

            if (! oauth.isSignedIn())
                state = State::signedOut;
        }
    }

    sendChangeMessage();
}

//==============================================================================
AppServices::AppServices()
    : youtube ([this] (juce::String& token, bool forceRefresh)
               {
                   if (forceRefresh)
                       oauth.invalidateAccessToken();

                   return oauth.getAccessToken (token);
               })
{
}

} // namespace beatdrop
