#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

class ThroughputCalc : public Application
{
public:
    ThroughputCalc() : m_rxBytes(0) {}
    virtual ~ThroughputCalc() {}

    void Setup(Ptr<Socket> socket)
    {
        m_socket = socket;
        m_socket->SetRecvCallback(MakeCallback(&ThroughputCalc::HandleRead, this));
    }

    void StartApplication() override
    {
        m_running = true;
    }
    void StopApplication() override
    {
        m_running = false;
    }

    void HandleRead(Ptr<Socket> socket)
    {
        while (Ptr<Packet> packet = socket->Recv())
        {
            m_rxBytes += packet->GetSize();
        }
    }
    uint64_t GetRxBytes() const { return m_rxBytes; }
private:
    Ptr<Socket> m_socket;
    uint64_t m_rxBytes;
    bool m_running = false;
};

int main(int argc, char *argv[])
{
    uint32_t nSta = 4;  // Total stations (clients), at least 2
    double simulationTime = 10.0;

    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    NodeContainer wifiStaNodes;
    wifiStaNodes.Create(nSta);

    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    // Create Wi-Fi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("ErpOfdmRate54Mbps"), "ControlMode", StringValue("ErpOfdmRate24Mbps"));

    // Set up MACs
    Ssid ssid = Ssid("wifi-infra");

    WifiMacHelper mac;
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, wifiStaNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(5.0),
                                 "DeltaY", DoubleValue(5.0),
                                 "GridWidth", UintegerValue(3),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes);
    mobility.Install(wifiApNode);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // UDP Server: on Station 0
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiStaNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(simulationTime));

    // UDP Client: on Station 1, send to station 0
    UdpClientHelper udpClient(staInterfaces.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(320));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.03))); // ~33 packets/sec
    udpClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = udpClient.Install(wifiStaNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(simulationTime));

    // Create socket for throughput monitoring on server node (station 0)
    Ptr<Socket> recvSocket = Socket::CreateSocket(wifiStaNodes.Get(0), UdpSocketFactory::GetTypeId());
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), port);
    recvSocket->Bind(local);

    Ptr<ThroughputCalc> throughputApp = CreateObject<ThroughputCalc>();
    throughputApp->Setup(recvSocket);
    wifiStaNodes.Get(0)->AddApplication(throughputApp);
    throughputApp->SetStartTime(Seconds(1.0));
    throughputApp->SetStopTime(Seconds(simulationTime));

    // Enable tracing
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11);
    phy.EnablePcap("wifi-ap", apDevice.Get(0));
    phy.EnablePcap("wifi-sta1", staDevices.Get(0));
    phy.EnablePcap("wifi-sta2", staDevices.Get(1));

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-trace.tr"));

    // Run simulation
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Throughput calculation at server
    double rxBytes = throughputApp->GetRxBytes();
    double throughput = (rxBytes * 8.0) / (simulationTime - 1.0) / 1e6; // Mbit/s, account for traffic start at t=1s

    std::cout << "UDP server received " << rxBytes << " bytes in "
              << (simulationTime - 1.0) << " seconds. Throughput: "
              << throughput << " Mbps" << std::endl;

    Simulator::Destroy();
    return 0;
}