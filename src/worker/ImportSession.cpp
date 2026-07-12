#include "worker/ImportSession.h"

namespace loupe::worker {

ImportSession::ImportSession(const std::uint64_t requestId, QObject* parent)
    : QObject(parent)
    , requestId_(requestId)
{
}

} // namespace loupe::worker
