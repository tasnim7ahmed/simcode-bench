#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("RateAdaptiveWifi");

uint64_t lastTotalRx = 0;
double lastDistance = 0.0;
double throughput = 0.0;
std::vector<double> distanceVec;
std::vector<double> throughputVec;
Ptr<OutputStreamWrapper> rateChangeLog;

void
ThroughputMonitor(Ptr<Application> app, Ptr<Node> apNode, Ptr<Node> staNode, Ptr<PacketSink> sink, double interval, bool enableLog)
{
    Simulator::Schedule(Seconds(interval), &ThroughputMonitor, app, apNode, staNode, sink, interval, enableLog);
    uint64_t totalRx = sink->GetTotalRx();
    double cur = (totalRx - lastTotalRx) * 8.0 / (interval * 1e6); // Mbps
    lastTotalRx = totalRx;

    Vector apPos = apNode->GetObject<MobilityModel>()->GetPosition();
    Vector staPos = staNode->GetObject<MobilityModel>()->GetPosition();
    double dist = std::sqrt(std::pow(apPos.x - staPos.x,2) + std::pow(apPos.y - staPos.y,2) + std::pow(apPos.z - staPos.z,2));

    distanceVec.push_back(dist);
    throughputVec.push_back(cur);

    if (enableLog)
    {
        NS_LOG_UNCOND ("[THROUGHPUT] Time: " << Simulator::Now ().GetSeconds ()
                       << "s Distance: " << dist << " m Throughput: " << cur << " Mbps");
    }
}

void
RateChangeCallback(std::string context, uint64_t oldRate, uint64_t newRate)
{
    *rateChangeLog->GetStream() << Simulator::Now().GetSeconds()
                                << "\tOldRate=" << oldRate
                                << "\tNewRate=" << newRate << std::endl;
}

int
main(int argc, char *argv[])
{
    double distanceStep = 10.0;         // meters per step
    double timeIntervalStep = 1.0;      // seconds per step
    uint32_t numSteps = 10;
    bool enableLog = false;
    std::string rateAlgo = "MinstrelWifiManager";
    std::string gnuplotFile = "throughput-vs-distance.plt";
    std::string rateChangeFile = "rate-changes.log";

    CommandLine cmd;
    cmd.AddValue("distanceStep", "Distance step in meters", distanceStep);
    cmd.AddValue("timeIntervalStep", "Time step in seconds", timeIntervalStep);
    cmd.AddValue("numSteps", "Number of distance steps", numSteps);
    cmd.AddValue("enableLog", "Enable logging of rate changes", enableLog);
    cmd.AddValue("gnuplotFile", "Output gnuplot file name", gnuplotFile);
    cmd.AddValue("rateChangeFile", "Rate change log file", rateChangeFile);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue("2347"));
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue("OfdmRate6Mbps"));

    NodeContainer wifiStaNode;
    wifiStaNode.Create(1);
    NodeContainer wifiApNode;
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager(rateAlgo);

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
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));    // AP at (0,0,0)
    positionAlloc->Add(Vector(distanceStep, 0.0, 0.0)); // STA starting at (distanceStep, 0, 0)
    mobility.SetPositionAllocator(positionAlloc);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNode);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(wifiApNode);
    stack.Install(wifiStaNode);

    // Assign IP address
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface, apInterface;
    apInterface = address.Assign(apDevice);
    staInterface = address.Assign(staDevice);

    // UDP CBR App - AP sends to STA
    uint16_t port = 9;
    ApplicationContainer serverApp, clientApp;

    UdpServerHelper server(port);
    serverApp = server.Install(wifiStaNode.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(numSteps * timeIntervalStep + 3));

    UdpClientHelper client(staInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0 / (400.0 * 1e6 / 1024 / 8)))); // 400Mbps
    client.SetAttribute("PacketSize", UintegerValue(1024));
    clientApp = client.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(numSteps * timeIntervalStep + 2));

    // Setup rate change logging if enabled
    Ptr<PacketSink> sink = DynamicCast<PacketSink>(serverApp.Get(0));
    if (enableLog)
    {
        LogComponentEnable("RateAdaptiveWifi", LOG_LEVEL_INFO);
        AsciiTraceHelper ascii;
        rateChangeLog = ascii.CreateFileStream(rateChangeFile);
        // Connect trace source (Minstrel reports MSRs on station)
        Config::Connect("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/RemoteStationManager/TxRateChanged",
                        MakeCallback(&RateChangeCallback));
    }

    // Schedule STA movement steps
    Ptr<ConstantPositionMobilityModel> staMob =
        wifiStaNode.Get(0)->GetObject<ConstantPositionMobilityModel>();

    for (uint32_t i = 0; i < numSteps; ++i)
    {
        double pos = distanceStep * (i+1);
        Simulator::Schedule(Seconds(timeIntervalStep * i),
            [staMob, pos]()
            {
                staMob->SetPosition(Vector(pos, 0.0, 0.0));
            }
        );
    }

    // Throughput Monitor
    Simulator::Schedule(Seconds(0),
        &ThroughputMonitor, clientApp.Get(0),
        wifiApNode.Get(0), wifiStaNode.Get(0), sink, timeIntervalStep, enableLog);

    Simulator::Stop(Seconds(numSteps * timeIntervalStep + 3));
    Simulator::Run();

    // Output Gnuplot file
    Gnuplot plot (gnuplotFile.c_str());
    plot.SetTitle("Throughput vs Distance");
    plot.SetLegend("Distance (m)", "Throughput (Mbps)");
    Gnuplot2dDataset dataset("Throughput");
    for (size_t i = 0; i < distanceVec.size(); ++i)
    {
        dataset.Add(distanceVec[i], throughputVec[i]);
    }
    plot.AddDataset(dataset);
    std::ofstream plotFile(gnuplotFile.c_str());
    plot.GenerateOutput(plotFile);
    plotFile.close();

    Simulator::Destroy();

    return 0;
}