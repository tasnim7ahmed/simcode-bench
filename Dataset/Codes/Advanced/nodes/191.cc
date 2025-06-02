#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/energy-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WirelessSensorNetwork");

void RemainingEnergy(Ptr<BasicEnergySource> source)
{
    NS_LOG_UNCOND("Remaining energy: " << source->GetRemainingEnergy() << " J");
    if (source->GetRemainingEnergy() <= 0)
    {
        NS_LOG_UNCOND("Node has run out of energy");
    }
}

int main (int argc, char *argv[])
{
    uint32_t gridSize = 3; // 3x3 grid of nodes
    double totalEnergy = 100.0; // Initial energy in Joules
    double simulationTime = 50.0; // seconds
    double dataRate = 1.0; // packets per second

    // 1. Create nodes
    NodeContainer sensorNodes;
    sensorNodes.Create (gridSize * gridSize);

    // 2. Configure Wifi PHY and MAC
    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    phy.SetChannel (channel.Create ());

    WifiMacHelper mac;
    Ssid ssid = Ssid ("wsn-grid");
    mac.SetType ("ns3::AdhocWifiMac", "Ssid", SsidValue (ssid));

    NetDeviceContainer devices = wifi.Install (phy, mac, sensorNodes);

    // 3. Set up mobility model (grid)
    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                    "MinX", DoubleValue (0.0),
                                    "MinY", DoubleValue (0.0),
                                    "DeltaX", DoubleValue (50.0),
                                    "DeltaY", DoubleValue (50.0),
                                    "GridWidth", UintegerValue (gridSize),
                                    "LayoutType", StringValue ("RowFirst"));
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (sensorNodes);

    // 4. Install energy model
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set ("BasicEnergySourceInitialEnergyJ", DoubleValue (totalEnergy));
    EnergySourceContainer sources = energySourceHelper.Install (sensorNodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install (devices, sources);

    // 5. Set up the Internet stack
    InternetStackHelper internet;
    internet.Install (sensorNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

    // 6. Install applications
    uint16_t sinkPort = 9;
    Address sinkAddress (InetSocketAddress (interfaces.GetAddress (0), sinkPort)); // First node is the sink

    PacketSinkHelper sinkHelper ("ns3::UdpSocketFactory", sinkAddress);
    ApplicationContainer sinkApp = sinkHelper.Install (sensorNodes.Get (0)); // Sink installed on node 0
    sinkApp.Start (Seconds (0.0));
    sinkApp.Stop (Seconds (simulationTime));

    OnOffHelper clientHelper ("ns3::UdpSocketFactory", sinkAddress);
    clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute ("DataRate", DataRateValue (DataRate ("1kbps")));
    clientHelper.SetAttribute ("PacketSize", UintegerValue (50));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < sensorNodes.GetN (); ++i) // All nodes except the sink send data
    {
        clientApps.Add (clientHelper.Install (sensorNodes.Get (i)));
    }
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (simulationTime));

    // 7. Trace remaining energy
    for (uint32_t i = 0; i < sources.GetN (); ++i)
    {
        Ptr<BasicEnergySource> basicSource = DynamicCast<BasicEnergySource> (sources.Get (i));
        basicSource->TraceConnectWithoutContext ("RemainingEnergy", MakeCallback (&RemainingEnergy));
    }

    // 8. Enable pcap tracing
    phy.EnablePcapAll ("wsn-grid");

    // 9. Run simulation
    Simulator::Stop (Seconds (simulationTime));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}

