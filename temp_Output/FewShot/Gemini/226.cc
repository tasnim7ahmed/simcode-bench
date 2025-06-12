#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {

    bool verbose = false;
    bool tracing = false;
    uint32_t numNodes = 10;
    double simulationTime = 50;
    double transmissionRange = 50;

    CommandLine cmd;
    cmd.AddValue("numNodes", "Number of sensor nodes", numNodes);
    cmd.AddValue("simulationTime", "Simulation runtime in seconds", simulationTime);
    cmd.AddValue("verbose", "Enable logging", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    Config::SetDefault("ns3::WifiMac::Ssid", StringValue("ns-3-ssid"));

    NodeContainer nodes;
    nodes.Create(numNodes + 1); // numNodes sensors + 1 sink

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("50.0"),
                                  "Y", StringValue("50.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=30.0]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Energy Model
    BasicEnergySourceHelper basicSourceHelper;
    EnergySourceContainer sources = basicSourceHelper.Install(nodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0197));
    radioEnergyHelper.Set("SleepCurrentA", DoubleValue(0.0000015));
    radioEnergyHelper.Set("TransitionCurrentA", DoubleValue(0.0000015));
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(devices, sources);

    // Sink application (on node numNodes)
    uint16_t sinkPort = 9;
    UdpServerHelper sink(sinkPort);
    ApplicationContainer sinkApps = sink.Install(nodes.Get(numNodes));
    sinkApps.Start(Seconds(1.0));
    sinkApps.Stop(Seconds(simulationTime));

    // Client application (on all sensor nodes)
    UdpClientHelper client(interfaces.GetAddress(numNodes), sinkPort);
    client.SetAttribute("MaxPackets", UintegerValue(1000));
    client.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes; ++i) {
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime - 1));

    if (tracing) {
        PointToPointHelper p2p;
        p2p.EnablePcapAll("olsr-wpan");
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}