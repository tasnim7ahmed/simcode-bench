#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/animation-interface.h"
#include "ns3/config-store-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    // Simulation parameters
    double simDuration = 10.0; // seconds
    uint32_t numVehicles = 5;
    uint32_t serverNodeIndex = 0;
    uint16_t port = 9;
    double interval = 1.0; // seconds

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    // Create nodes for vehicles
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Setup mobility model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    // Start and set velocity for each vehicle
    for (uint32_t i = 0; i < numVehicles; ++i) {
        Ptr<Node> node = vehicles.Get(i);
        Ptr<MobilityModel> mobilityModel = node->GetObject<MobilityModel>();
        mobilityModel->SetPosition(Vector(i * 20.0, 0.0, 0.0));
        mobilityModel->GetObject<ConstantVelocityMobilityModel>()->SetVelocity(Vector(10.0 + i, 0.0, 0.0));
    }

    // Configure Wi-Fi with IEEE 802.11p
    WifiMacHelper wifiMac;
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211_10MHZ);

    // Set default rates and channel width
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                       "DataMode", StringValue("OfdmRate6_5MbpsBW10MHz"),
                                       "ControlMode", StringValue("OfdmRate6_5MbpsBW10MHz"));

    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifiHelper.Install(wifiMac, vehicles);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Server application on the first node
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(vehicles.Get(serverNodeIndex));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simDuration));

    // Clients on other nodes
    UdpClientHelper client(interfaces.GetAddress(serverNodeIndex), port);
    client.SetAttribute("Interval", TimeValue(Seconds(interval)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < numVehicles; ++i) {
        clientApps.Add(client.Install(vehicles.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simDuration));

    // Enable NetAnim visualization
    AnimationInterface anim("vanet_simulation.xml");
    anim.EnablePacketMetadata(true);

    // Flow monitor to collect metrics
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();

    // Print metrics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    double totalDelay = 0.0;
    uint32_t totalTxPackets = 0;
    uint32_t totalRxPackets = 0;

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        if (t.sourcePort == port || t.destinationPort == port) {
            NS_LOG_UNCOND("Flow ID: " << i->first << " Src Addr: " << t.sourceAddress <<
                          " Dst Addr: " << t.destinationAddress <<
                          " Tx Packets: " << i->second.txPackets <<
                          " Rx Packets: " << i->second.rxPackets <<
                          " Lost Packets: " << i->second.lostPackets <<
                          " Avg Delay: " << i->second.delaySum.GetSeconds() / (double)(std::max(1u, i->second.rxPackets)) << " s");

            totalTxPackets += i->second.txPackets;
            totalRxPackets += i->second.rxPackets;
            totalDelay += i->second.delaySum.GetSeconds();
        }
    }

    double packetDeliveryRatio = (totalTxPackets > 0) ? ((double)totalRxPackets / totalTxPackets) : 0.0;
    double avgEndToEndDelay = (totalRxPackets > 0) ? (totalDelay / totalRxPackets) : 0.0;

    NS_LOG_UNCOND("=== Overall Metrics ===");
    NS_LOG_UNCOND("Packet Delivery Ratio: " << packetDeliveryRatio * 100 << "%");
    NS_LOG_UNCOND("Average End-to-End Delay: " << avgEndToEndDelay << " seconds");

    Simulator::Destroy();
    return 0;
}