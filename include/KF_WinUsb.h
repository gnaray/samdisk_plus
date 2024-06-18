#pragma once

#include "config.h" // IWYU pragma: keep

#ifdef HAVE_WINUSB

#include "Platform.h"
#include "winusb_defs.h" // just the definitions we need
#include "KryoFlux.h"

class KF_WinUsb final : public KryoFlux
{
public:
    KF_WinUsb(HANDLE hdev, WINUSB_INTERFACE_HANDLE winusb, WINUSB_INTERFACE_HANDLE interface1);
    ~KF_WinUsb();

    static std::unique_ptr<KryoFlux> Open();
    static std::string GetDevicePath();

private:
    std::string KF_WinUsb::GetProductName() override;

    std::string Control(int req, int index, int value) override;
    int Read(void* buf, int len) override;
    int Write(const void* buf, int len) override;

    int ReadAsync(void* buf, int len) override;
    void StartAsyncRead() override;
    void StopAsyncRead() override;

    HANDLE m_hdev = nullptr;
    WINUSB_INTERFACE_HANDLE m_winusb = nullptr;
    WINUSB_INTERFACE_HANDLE m_interface1 = nullptr;
};

#endif // HAVE_WINUSB
