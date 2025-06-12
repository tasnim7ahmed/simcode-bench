#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/gnuplot.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("RateAdaptiveWifiSimulation");

int main(int argc, char *argv[]) {
    uint32_t packetSize = 1472; // bytes
    std::string dataRate = "400Mbps";
    double simulationTime = 60.0; // seconds
    double initialDistance = 1.0; // meters
    double distanceStep = 5.0; // meters
    double stepInterval = 5.0; // seconds
    bool enableLog = false;
    std::string outputFilename = "throughput-vs-distance.dat";

    CommandLine cmd;
    cmd.AddValue("packetSize", "size of application packet sent", packetSize);
    cmd.AddValue("dataRate", "CBR data rate", dataRate);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("initialDistance", "Initial distance between AP and STA (meters)", initialDistance);
    cmd.AddValue("distanceStep", "Distance increment step (meters)", distanceStep);
    cmd.AddValue("stepInterval", "Time interval for each distance step (seconds)", stepInterval);
    cmd.AddValue("enableLog", "Enable logging output", enableLog);
    cmd.AddValue("outputFilename", "Output Gnuplot data filename", outputFilename);
    cmd.Parse(argc, argv);

    if (enableLog) {
        LogComponentEnable("RateAdaptiveWifiSimulation", LOG_LEVEL_INFO);
        LogComponentEnable("MinstrelWifiManager", LOG_LEVEL_INFO);
    }

    NodeContainer wifiStaNode;
    NodeContainer wifiApNode;
    wifiStaNode.Create(1);
    wifiApNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    WifiHelper wifi;

    wifi.SetStandard(WIFI_STANDARD_80211ac);
    wifi.SetRemoteStationManager("ns3::MinstrelWifiManager"); // STA uses Minstrel

    mac.SetType("ns3::StaWifiMac");
    phy.Set("TxPowerStart", DoubleValue(20));
    phy.Set("TxPowerEnd", DoubleValue(20));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, wifiStaNode);

    mac.SetType("ns3::ApWifiMac");
    wifi.SetRemoteStationManager("ns3::ArfWifiManager"); // AP can use any manager
    phy.Set("TxPowerStart", DoubleValue(20));
    phy.Set("TxPowerEnd", DoubleValue(20));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, wifiApNode);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0)); // AP fixed at origin
    positionAlloc->Add(Vector(initialDistance, 0.0, 0.0)); // STA starts here
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiApNode);
    mobility.Install(wifiStaNode);

    InternetStackHelper stack;
    stack.Install(wifiStaNode);
    stack.Install(wifiApNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

    // UDP Echo Server on STA
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(wifiStaNode.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    // CBR traffic from AP to STA
    UdpClientHelper cbrClient(staInterface.GetAddress(0), 9);
    cbrClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    cbrClient.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
    cbrClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApp = cbrClient.Install(wifiApNode.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(simulationTime));

    Config::Connect("/NodeList/1/ApplicationList/0/$ns3::UdpEchoServer/Rx", MakeCallback([](std::string context, Ptr<const Packet> p, const Address&) {
        NS_LOG_INFO("Received packet at server: " << p->GetSize() << " bytes");
    }));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream(outputFilename.c_str());
    Gnuplot gnuplot(outputFilename);
    gnuplot.SetTitle("Throughput vs Distance");
    gnuplot.SetXTitle("Distance (m)");
    gnuplot.SetYTitle("Throughput (Mbps)");

    double currentDistance = initialDistance;
    uint32_t totalBytes = 0;

    auto throughputCallback = [&totalBytes](std::string context, Ptr<const Packet> p, const Address&) {
        totalBytes += p->GetSize();
    };

    Config::ConnectWithoutContext("/NodeList/1/ApplicationList/0/$ns3::UdpEchoServer/Rx", MakeCallback(throughputCallback));

    for (double t = 0; t < simulationTime; t += stepInterval) {
        Simulator::Schedule(Seconds(t), [currentDistance, wifiStaNode]() {
            Ptr<MobilityModel> mob = wifiStaNode.Get(0)->GetObject<MobilityModel>();
            Vector pos = mob->GetPosition();
            pos.x = currentDistance;
            mob->SetPosition(Vector(currentDistance, 0.0, 0.0));
            NS_LOG_INFO("Setting STA position to distance: " << currentDistance << " m");
        });

        Simulator::Schedule(Seconds(t + stepInterval), [&, currentDistance]() {
            double duration = stepInterval;
            double throughput = (totalBytes * 8.0 / duration) / 1e6;
            NS_LOG_INFO("Throughput at distance " << currentDistance << " m: " << throughput << " Mbps");
            *stream->GetStream() << currentDistance << " " << throughput << std::endl;
            totalBytes = 0;
            currentDistance += distanceStep;
        });
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}