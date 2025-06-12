#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211g);

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;

    // STA: RandomWalk2dMobility
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)));
    mobility.Install(wifiStaNode);

    // AP: stationary
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface, apInterface;
    staInterface = address.Assign(staDevice);
    apInterface = address.Assign(apDevice);

    uint16_t port = 9;

    // Install a packet sink on the AP
    Address sinkAddress(InetSocketAddress(apInterface.GetAddress(0), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(wifiApNode.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(15.0));

    // Create the sender application at STA
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(wifiStaNode.Get(0), TcpSocketFactory::GetTypeId());

    // Custom application to send 10 packets of 1024 bytes at 1 second intervals
    class TcpApp : public Application
    {
    public:
        TcpApp() : m_socket(0), m_sendEvent(), m_running(false), m_packetsSent(0) {}
        void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, Time interval)
        {
            m_socket = socket;
            m_peer = address;
            m_packetSize = packetSize;
            m_nPackets = nPackets;
            m_interval = interval;
        }
        virtual void StartApplication()
        {
            m_running = true;
            m_packetsSent = 0;
            m_socket->Connect(m_peer);
            SendPacket();
        }
        virtual void StopApplication()
        {
            m_running = false;
            if (m_sendEvent.IsRunning())
                Simulator::Cancel(m_sendEvent);
            if (m_socket)
                m_socket->Close();
        }
    private:
        void SendPacket()
        {
            Ptr<Packet> packet = Create<Packet>(m_packetSize);
            m_socket->Send(packet);
            ++m_packetsSent;
            if (m_packetsSent < m_nPackets)
            {
                m_sendEvent = Simulator::Schedule(m_interval, &TcpApp::SendPacket, this);
            }
        }

        Ptr<Socket>     m_socket;
        Address         m_peer;
        uint32_t        m_packetSize;
        uint32_t        m_nPackets;
        Time            m_interval;
        EventId         m_sendEvent;
        bool            m_running;
        uint32_t        m_packetsSent;
    };

    Ptr<TcpApp> app = CreateObject<TcpApp>();
    app->Setup(ns3TcpSocket, sinkAddress, 1024, 10, Seconds(1.0));
    wifiStaNode.Get(0)->AddApplication(app);
    app->SetStartTime(Seconds(1.0));
    app->SetStopTime(Seconds(12.0));

    Simulator::Stop(Seconds(15.0));

    phy.EnablePcap("wifi-tcp", apDevice.Get(0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}