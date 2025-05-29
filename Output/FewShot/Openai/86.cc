#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiBasicSenderReceiverExample");

class WifiSenderApp : public Application
{
public:
    WifiSenderApp()
        : m_socket(0),
          m_sendEvent(),
          m_sent(0),
          m_running(false)
    {}

    void SetDestination(Address address, uint16_t port)
    {
        m_peerAddress = address;
        m_peerPort = port;
    }

    void SetPacketSize(uint32_t size)
    {
        m_packetSize = size;
    }

    void SetNumPackets(uint32_t num)
    {
        m_numPackets = num;
    }

    void SetInterval(Time interval)
    {
        m_interval = interval;
    }

protected:
    virtual void StartApplication() override
    {
        m_running = true;
        m_sent = 0;

        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            m_socket->Bind();
        }
        m_socket->Connect(InetSocketAddress(InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4(), m_peerPort));

        SendPacket();
    }

    virtual void StopApplication() override
    {
        m_running = false;
        if (m_sendEvent.IsRunning())
        {
            Simulator::Cancel(m_sendEvent);
        }
        if (m_socket)
        {
            m_socket->Close();
            m_socket = 0;
        }
    }

    void SendPacket()
    {
        if (m_sent < m_numPackets)
        {
            Ptr<Packet> packet = Create<Packet>(m_packetSize);

            // Add timestamp to packet: store timestamp at beginning of packet
            Time now = Simulator::Now();
            uint64_t timeNano = now.GetNanoSeconds();
            uint8_t tsbuf[8];
            for (int i = 0; i < 8; ++i)
                tsbuf[i] = (timeNano >> (56 - 8 * i)) & 0xff;
            packet->AddHeader(Buffer(tsbuf, 8));

            m_socket->Send(packet);

            ++m_sent;
            NS_LOG_INFO("Sender: Sent packet " << m_sent << " at " << now.GetSeconds() << "s");
            m_sendEvent = Simulator::Schedule(m_interval, &WifiSenderApp::SendPacket, this);
        }
    }

private:
    Ptr<Socket> m_socket;
    Address m_peerAddress;
    uint16_t m_peerPort;
    uint32_t m_packetSize{1024};
    uint32_t m_numPackets{10};
    uint32_t m_sent;
    Time m_interval{Seconds(1.0)};
    EventId m_sendEvent;
    bool m_running;
};

class WifiReceiverApp : public Application
{
public:
    WifiReceiverApp()
        : m_socket(0), m_port(0), m_received(0), m_totalDelay(Seconds(0.0))
    {}

    void SetListeningPort(uint16_t port)
    {
        m_port = port;
    }

protected:
    virtual void StartApplication() override
    {
        if (!m_socket)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            m_socket->Bind(local);
        }
        m_socket->SetRecvCallback(MakeCallback(&WifiReceiverApp::HandleRead, this));
        m_received = 0;
        m_totalDelay = Seconds(0.0);
        m_totalSqDelay = 0;
    }

    virtual void StopApplication() override
    {
        if (m_socket)
        {
            m_socket->Close();
            m_socket = 0;
        }
        if (m_received > 0)
        {
            Time avgDelay = m_totalDelay / m_received;
            Time stddev;
            if (m_received > 1)
            {
                double variance = (double)m_totalSqDelay / m_received - avgDelay.GetSeconds() * avgDelay.GetSeconds();
                stddev = Seconds(std::sqrt(variance));
            }
            else
            {
                stddev = Seconds(0);
            }
            NS_LOG_UNCOND("Receiver: Received " << m_received << " packets");
            NS_LOG_UNCOND("Receiver: Average delay = " << avgDelay.GetMilliSeconds() << " ms");
            NS_LOG_UNCOND("Receiver: Stddev delay = " << stddev.GetMilliSeconds() << " ms");
        }
        else
        {
            NS_LOG_UNCOND("Receiver: No packets received.");
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        while (Ptr<Packet> packet = socket->Recv())
        {
            uint8_t timebuf[8];
            if (packet->GetSize() >= 8)
            {
                packet->CopyData(timebuf, 8);
                uint64_t sendNano = 0;
                for (int i = 0; i < 8; ++i)
                    sendNano = (sendNano << 8) | timebuf[i];
                Time sendTime = NanoSeconds(sendNano);
                Time now = Simulator::Now();
                Time delay = now - sendTime;
                ++m_received;
                m_totalDelay += delay;
                m_totalSqDelay += delay.GetSeconds() * delay.GetSeconds();

                NS_LOG_INFO("Receiver: Received packet #" << m_received <<
                            " Delay = " << delay.GetMilliSeconds() << " ms at " << now.GetSeconds() << "s");
            }
            else
            {
                NS_LOG_WARN("Receiver: Received packet without timestamp");
            }
        }
    }

private:
    Ptr<Socket> m_socket;
    uint16_t m_port;
    uint32_t m_received;
    Time m_totalDelay;
    double m_totalSqDelay;
};


int main(int argc, char *argv[])
{
    // Logging
    LogComponentEnable("WifiBasicSenderReceiverExample", LOG_LEVEL_INFO);

    // Defaults
    uint32_t numPackets = 20;
    uint32_t packetSize = 512;
    double interval = 0.5;
    uint16_t port = 5000;

    // Create two nodes: sender and receiver
    NodeContainer nodes;
    nodes.Create(2);

    // Set up Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(phy, mac, nodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, nodes.Get(1));

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces.Add(address.Assign(staDevice));
    interfaces.Add(address.Assign(apDevice));

    // WifiSenderApp
    Ptr<WifiSenderApp> senderApp = CreateObject<WifiSenderApp>();
    senderApp->SetPacketSize(packetSize);
    senderApp->SetNumPackets(numPackets);
    senderApp->SetInterval(Seconds(interval));
    senderApp->SetDestination(interfaces.GetAddress(1), port);
    nodes.Get(0)->AddApplication(senderApp);
    senderApp->SetStartTime(Seconds(1.0));
    senderApp->SetStopTime(Seconds(1.0 + numPackets * interval + 2.0));

    // WifiReceiverApp
    Ptr<WifiReceiverApp> receiverApp = CreateObject<WifiReceiverApp>();
    receiverApp->SetListeningPort(port);
    nodes.Get(1)->AddApplication(receiverApp);
    receiverApp->SetStartTime(Seconds(0.0));
    receiverApp->SetStopTime(Seconds(1.0 + numPackets * interval + 4.0));

    Simulator::Stop(Seconds(1.0 + numPackets * interval + 5.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}