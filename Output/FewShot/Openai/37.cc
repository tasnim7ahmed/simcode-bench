#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/flow-monitor-helper.h"
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiRateControlComparison");

class PowerRateTracer
{
public:
    PowerRateTracer(std::string outPowerFile, std::string outRateFile)
        : m_powerFile(outPowerFile, std::ofstream::out), m_rateFile(outRateFile, std::ofstream::out)
    {
        m_powerFile << "#Time(s)\tDistance(m)\tTxPower(W)\n";
        m_rateFile  << "#Time(s)\tDistance(m)\tTxRate(Mbps)\n";
    }
    void Log(double time, double distance, double txPower, double txRate)
    {
        m_powerFile << time << "\t" << distance << "\t" << txPower << "\n";
        m_rateFile  << time << "\t" << distance << "\t" << txRate/1e6 << "\n";
    }
    ~PowerRateTracer()
    {
        m_powerFile.close();
        m_rateFile.close();
    }
private:
    std::ofstream m_powerFile, m_rateFile;
};

void
AdvanceStaPosition(Ptr<MobilityModel> staMob, double step, uint32_t steps, uint32_t &curr, Time interval, double apX,
                   Ptr<NetDevice> apDev, Ptr<NetDevice> staDev, PowerRateTracer* tracer)
{
    if (curr >= steps) return;
    curr++;
    Vector pos = staMob->GetPosition();
    pos.x += step;
    staMob->SetPosition(pos);

    Ptr<WifiNetDevice> apWifi = DynamicCast<WifiNetDevice>(apDev);
    Ptr<WifiTxVector> txVector = Create<WifiTxVector>();
    // We get current tx power from WifiPhy
    Ptr<WifiPhy> phy = apWifi->GetPhy();
    double txPower = phy->GetTxPowerStart();
    double txRate = phy->GetMode().GetDataRate();

    double distance = pos.x - apX;
    tracer->Log(Simulator::Now().GetSeconds(), distance, txPower, txRate);

    Simulator::Schedule(interval, &AdvanceStaPosition, staMob, step, steps, std::ref(curr), interval, apX, apDev, staDev, tracer);
}

double
CalculateThroughput(FlowMonitorHelper& flowmon, Ptr<FlowMonitor> monitor, double duration)
{
    monitor->CheckForLostPackets();
    double throughput = 0.0;
    auto stats = monitor->GetFlowStats();
    for(auto it = stats.begin(); it != stats.end(); ++it)
    {
        double bytes = it->second.rxBytes;
        throughput += (bytes * 8) / duration / 1e6; // Mbps
    }
    return throughput;
}

int main(int argc, char *argv[])
{
    std::string wifiManager = "ParfWifiManager";
    uint32_t rtsThreshold = 2347;
    std::string throughputFile = "throughput.dat";
    std::string txPowerFile = "txpower.dat";
    std::string txRateFile = "txrate.dat";
    double simTime = 30.0;
    double stepDistance = 1.0;
    double interval = 1.0;

    CommandLine cmd;
    cmd.AddValue("manager", "WiFi rate control manager (ParfWifiManager, AparfWifiManager, RrpaaWifiManager)", wifiManager);
    cmd.AddValue("rts", "RTS/CTS threshold (bytes)", rtsThreshold);
    cmd.AddValue("throughputFile", "Output throughput filename", throughputFile);
    cmd.AddValue("txPowerFile", "Output txpower filename", txPowerFile);
    cmd.AddValue("txRateFile", "Output txrate filename", txRateFile);
    cmd.AddValue("simTime", "Simulation time (seconds)", simTime);
    cmd.Parse(argc, argv);

    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);

    NodeContainer wifiStaNode, wifiApNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager(wifiManager, "RtsCtsThreshold", UintegerValue(rtsThreshold));

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-ssid");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;

    Ptr<ListPositionAllocator> posAlloc = CreateObject<ListPositionAllocator>();
    posAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP at origin
    posAlloc->Add(Vector(stepDistance, 0.0, 0.0)); // STA at 1m
    mobility.SetPositionAllocator(posAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNode);

    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface, staInterface;
    NetDeviceContainer allDevices;
    allDevices.Add(apDevice.Get(0));
    allDevices.Add(staDevice.Get(0));
    Ipv4InterfaceContainer interfaces = address.Assign(allDevices);

    uint16_t port = 9;
    // Application: UDP server at STA, client at AP (AP sends traffic)
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(wifiStaNode.Get(0));
    serverApp.Start(Seconds(0.1));
    serverApp.Stop(Seconds(simTime));

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295u));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0/6750))); // 1024*6750*8 bits = 54Mbps
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApp = client.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(0.2));
    clientApp.Stop(Seconds(simTime));

    // FlowMonitor for throughput
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    // Schedule mobility and trace collection
    Ptr<MobilityModel> staMob = wifiStaNode.Get(0)->GetObject<MobilityModel>();
    double apX = 0.0;
    uint32_t numSteps = static_cast<uint32_t>(simTime/interval);
    uint32_t currStep = 0;
    PowerRateTracer tracer(txPowerFile, txRateFile);

    Simulator::Schedule(Seconds(0.0), &AdvanceStaPosition, staMob, stepDistance, numSteps, std::ref(currStep), Seconds(interval), apX,
                        apDevice.Get(0), staDevice.Get(0), &tracer);

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // --- Throughput Computation ---
    double throughput = CalculateThroughput(flowmon, monitor, simTime);

    // Output for plotting: Throughput vs distance (since one step per second)
    std::ofstream outThroughput(throughputFile, std::ofstream::out);
    outThroughput << "#Distance(m)\tThroughput(Mbps)\n";
    for(uint32_t i = 0; i < numSteps; ++i)
    {
        outThroughput << (i+1)*stepDistance << "\t" << throughput << "\n";
    }
    outThroughput.close();

    Simulator::Destroy();
    return 0;
}