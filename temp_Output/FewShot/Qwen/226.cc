#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time
    double simTime = 50.0;

    // Enable OLSR logging
    LogComponentEnable("OlsrProtocol", LOG_LEVEL_INFO);

    // Create nodes (1 sink + 4 sensors)
    NodeContainer nodes;
    nodes.Create(5);
    Ptr<Node> sinkNode = nodes.Get(0);

    // Setup wireless channel and phy
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Setup MAC layer
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    // Setup Wi-Fi
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility model: static positions for simplicity
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Install Internet stack with OLSR routing
    OlsrHelper olsr;
    InternetStackHelper internet;
    internet.SetRoutingHelper(olsr);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Energy source setup
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(100.0));
    EnergySourceContainer sources = energySourceHelper.Install(nodes);

    // Device energy model
    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.35));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.35));
    radioEnergyHelper.Install(devices, sources);

    // Set up UDP echo server on the sink node (node 0)
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(sinkNode);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // Set up periodic UDP clients on sensor nodes (nodes 1-4)
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0)); // Unlimited packets
        echoClient.SetAttribute("Interval", TimeValue(Seconds(2.0)));
        echoClient.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer clientApp = echoClient.Install(nodes.Get(i));
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simTime));
    }

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}