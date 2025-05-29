#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiTcpExample");

int main(int argc, char *argv[])
{
    // Enable logging (optional)
    // LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Set simulation parameters
    uint32_t numPackets = 10;
    uint32_t packetSize = 1024;
    double interval = 1.0;
    std::string dataRate = "54Mbps";
    uint16_t port = 9;

    // Create nodes
    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Create and configure Wi-Fi channel and physical layer
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure Wi-Fi standard
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);

    // Set data rate and remote station manager
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("ErpOfdmRate54Mbps"),
                                "ControlMode", StringValue("ErpOfdmRate6Mbps"));

    // Configure MAC layer
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-wifi-ssid");

    // Configure STA (client)
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice;
    staDevice = wifi.Install(phy, mac, wifiStaNode);

    // Configure AP
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility configuration
    MobilityHelper mobility;

    // STA uses RandomWalk2dMobilityModel within 100 x 100 m² area
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)));
    mobility.Install(wifiStaNode);

    // AP is stationary at (50,50)
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    Ptr<MobilityModel> apMob = wifiApNode.Get(0)->GetObject<MobilityModel>();
    apMob->SetPosition(Vector(50.0, 50.0, 0.0));

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface, apInterface;
    staInterface = address.Assign(staDevice);
    apInterface = address.Assign(apDevice);

    // Install TCP server (AP listens on port 9)
    Address apLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", apLocalAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(wifiApNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(15.0));

    // Install TCP client (STA sends 10 packets to AP)
    OnOffHelper clientHelper("ns3::TcpSocketFactory",
                             InetSocketAddress(apInterface.GetAddress(0), port));
    clientHelper.SetAttribute("MaxBytes", UintegerValue(numPackets * packetSize));
    clientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
    clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    clientHelper.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    clientHelper.SetAttribute("StopTime", TimeValue(Seconds(1.0 + numPackets * interval)));

    ApplicationContainer clientApp = clientHelper.Install(wifiStaNode.Get(0));

    // Schedule packet sends manually to achieve 1 packet per second
    Ptr<Socket> srcSocket = Socket::CreateSocket(wifiStaNode.Get(0), TcpSocketFactory::GetTypeId());
    Address dstAddress = InetSocketAddress(apInterface.GetAddress(0), port);

    class Sender : public Object
    {
    public:
        Sender(Ptr<Socket> socket, Address address, uint32_t numPackets, uint32_t packetSize, double interval)
            : m_socket(socket), m_peer(address), m_numPackets(numPackets), m_packetSize(packetSize),
              m_interval(interval), m_sent(0)
        {}

        void Start()
        {
            m_socket->Connect(m_peer);
            SendPacket();
        }

        void SendPacket()
        {
            if (m_sent < m_numPackets)
            {
                m_socket->Send(Create<Packet>(m_packetSize));
                ++m_sent;
                Simulator::Schedule(Seconds(m_interval), &Sender::SendPacket, this);
            }
        }
    private:
        Ptr<Socket> m_socket;
        Address m_peer;
        uint32_t m_numPackets;
        uint32_t m_packetSize;
        double m_interval;
        uint32_t m_sent;
    };

    Ptr<Sender> sender = CreateObject<Sender>(srcSocket, dstAddress, numPackets, packetSize, interval);
    Simulator::Schedule(Seconds(1.0), &Sender::Start, sender);

    // Enable pcap tracing (optional)
    phy.EnablePcap("wifi-tcp-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-tcp-sta", staDevice.Get(0));

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}