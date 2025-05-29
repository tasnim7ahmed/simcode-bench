#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WsnOlsrSimulation");

int main(int argc, char *argv[])
{
    uint32_t numSensorNodes = 10;
    double simTime = 50.0;
    double txPowerStart = 16.0;
    double txPowerEnd = 16.0;
    double periodicInterval = 5.0;

    CommandLine cmd;
    cmd.AddValue("numSensorNodes", "Number of sensor nodes", numSensorNodes);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numSensorNodes + 1); // sensors + sink

    // Install mobility - sensors in grid, sink in center
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(20.0),
                                 "DeltaY", DoubleValue(20.0),
                                 "GridWidth", UintegerValue(5),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(nodes);

    // WiFi parameters
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiMacHelper mac;
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", DoubleValue(txPowerStart));
    phy.Set("TxPowerEnd", DoubleValue(txPowerEnd));

    Ssid ssid = Ssid("WSN-OLSR");
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    // Energy source and Wifi Radio Energy Model
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
    EnergySourceContainer energySources = energySourceHelper.Install(nodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0197));
    radioEnergyHelper.Install(devices, energySources);

    // Install Internet stack + OLSR
    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Install UDP server in the sink (last node)
    uint16_t port = 4000;
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(numSensorNodes));
    serverApps.Start(Seconds(0.5));
    serverApps.Stop(Seconds(simTime));

    // Install UDP client in each sensor node, targeting the sink
    for (uint32_t i = 0; i < numSensorNodes; ++i) {
        UdpClientHelper udpClient(interfaces.GetAddress(numSensorNodes), port);
        udpClient.SetAttribute("MaxPackets", UintegerValue(simTime / periodicInterval));
        udpClient.SetAttribute("Interval", TimeValue(Seconds(periodicInterval)));
        udpClient.SetAttribute("PacketSize", UintegerValue(64));
        ApplicationContainer clientApp = udpClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0 + i * 0.1));
        clientApp.Stop(Seconds(simTime));
    }

    // Trace remaining energy
    Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper>(&std::cout);
    for (uint32_t i = 0; i < numSensorNodes; ++i) {
        energySources.Get(i)->TraceConnectWithoutContext("RemainingEnergy",
            MakeBoundCallback([](Ptr<OutputStreamWrapper> stream, double oldVal, double newVal) {
                *stream->GetStream() << Simulator::Now().GetSeconds()
                                    << "s, Node energy: " << newVal << " J" << std::endl;
            }, stream));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}