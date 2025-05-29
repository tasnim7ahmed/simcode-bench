#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiUdpClientApMinimalExample");

// Application to log received UDP packets at the AP
class LoggingUdpServer : public Application
{
public:
    LoggingUdpServer() {}
    virtual ~LoggingUdpServer() {}

protected:
    virtual void StartApplication() override
    {
        if (m_socket == 0)
        {
            m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
            InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), m_port);
            m_socket->Bind(local);
            m_socket->SetRecvCallback(MakeCallback(&LoggingUdpServer::HandleRead, this));
        }
    }

    virtual void StopApplication() override
    {
        if (m_socket)
        {
            m_socket->Close();
            m_socket = 0;
        }
    }

    void HandleRead(Ptr<Socket> socket)
    {
        Ptr<Packet> packet;
        Address from;
        while ((packet = socket->RecvFrom(from)))
        {
            uint32_t pktSize = packet->GetSize();
            InetSocketAddress address = InetSocketAddress::ConvertFrom(from);
            NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "s: AP received " << pktSize <<
                " bytes from " << address.GetIpv4());
        }
    }

public:
    void SetPort(uint16_t port) { m_port = port; }

private:
    Ptr<Socket> m_socket = 0;
    uint16_t m_port = 9;
};

int main(int argc, char *argv[])
{
    // Enable logging for our component and UDP client
    LogComponentEnable("WifiUdpClientApMinimalExample", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    // Create two nodes: client and AP
    NodeContainer wifiNodes;
    wifiNodes.Create(2);
    Ptr<Node> clientNode = wifiNodes.Get(0);
    Ptr<Node> apNode = wifiNodes.Get(1);

    // Configure Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Use default 802.11n 2.4 GHz
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi-simple-ssid");

    // Set up client (STA)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, clientNode);

    // Set up access point (AP)
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, apNode);

    // Assign mobility models
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(wifiNodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(NetDeviceContainer(staDevice, apDevice));

    // Application port
    uint16_t serverPort = 9000;

    // Install custom UDP server app on AP node
    Ptr<LoggingUdpServer> serverApp = CreateObject<LoggingUdpServer>();
    serverApp->SetPort(serverPort);
    apNode->AddApplication(serverApp);
    serverApp->SetStartTime(Seconds(1.0));
    serverApp->SetStopTime(Seconds(10.0));

    // Install UDP client on client node to send packets to the AP
    UdpClientHelper clientHelper(interfaces.GetAddress(1), serverPort);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(20));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.3))); // Send every 300 ms
    clientHelper.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApp;
    clientApp = clientHelper.Install(clientNode);
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}