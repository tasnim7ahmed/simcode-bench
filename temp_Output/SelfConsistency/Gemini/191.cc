#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSN");

// Define a custom energy source container
class WsnEnergySourceContainer : public EnergySourceContainer
{
public:
    WsnEnergySourceContainer() : EnergySourceContainer() {}

    /**
     * \param i the index of the device for which to retrieve the energy source.
     * \returns the energy source associated with the i-th device, or a null
     * pointer if not available.
     */
    Ptr<EnergySource> Get(uint32_t i) const
    {
        if (i >= m_energySources.size())
        {
            return nullptr;
        }
        return m_energySources[i];
    }
};


int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("WSN", LOG_LEVEL_INFO);

    // Set simulation parameters
    uint32_t numNodes = 9; // Number of sensor nodes (arranged in a 3x3 grid)
    double simulationTime = 100; // Simulation time in seconds
    double packetInterval = 1.0;   // Packet generation interval in seconds
    uint32_t packetSize = 1024; // Packet size in bytes
    double initialEnergy = 100.0; // Initial energy of each sensor node in Joules
    double transmitPower = 0.1; // Transmit power consumption per packet (Joules)

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Create a sink node
    NodeContainer sinkNode;
    sinkNode.Create(1);

    // Combine nodes and sink node
    NodeContainer allNodes;
    allNodes.Add(nodes);
    allNodes.Add(sinkNode);

    // Configure wireless channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Create();
    phy.SetChannel(channel.Create());

    // Configure WiFi MAC layer
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("ns-3-wsn");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    // Install WiFi on sensor nodes
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes);

    // Configure AP (sink node)
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, sinkNode);

    // Configure mobility
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Set the sink node position
    Ptr<ListPositionAllocator> sinkPositionAlloc = CreateObject<ListPositionAllocator>();
    sinkPositionAlloc->Add(Vector(15.0, 15.0, 0.0)); // Position the sink near the center
    mobility.SetPositionAllocator(sinkPositionAlloc);
    mobility.Install(sinkNode);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(allNodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer sinkInterface = address.Assign(apDevices);

    // Enable IPv4 global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure energy model
    BasicEnergySourceHelper basicSourceHelper;
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergy));

    WsnEnergySourceContainer sources;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<BasicEnergySource> basicSourcePtr = basicSourceHelper.Install(node);
        sources.Add(basicSourcePtr);

        // Create a simple device energy model
        // (simplified, assuming fixed transmit power consumption)
        Ptr<DeviceEnergyModel> deviceEnergyModel = CreateObject<DeviceEnergyModel>();
        deviceEnergyModel->SetNode(node);
        deviceEnergyModel->SetEnergySource(basicSourcePtr);
        deviceEnergyModel->SetAttribute("TxCurrentA", DoubleValue(transmitPower / 3.7)); // Convert power to current (assuming 3.7V)
        basicSourcePtr->AppendDeviceEnergyModel(deviceEnergyModel);
    }

    // Install receiver on the sink node
    uint16_t port = 9;
    UdpServerHelper echoServer(port);
    ApplicationContainer sinkApp = echoServer.Install(sinkNode.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime));

    // Configure packet generation application
    UdpClientHelper echoClient(sinkInterface.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(static_cast<uint32_t>(simulationTime / packetInterval))); // Set MaxPackets dynamically
    echoClient.SetAttribute("Interval", TimeValue(Seconds(packetInterval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        clientApps.Add(echoClient.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Connect trace sinks to energy sources to track energy consumption
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<Node> node = nodes.Get(i);
        Ptr<EnergySource> energySource = sources.Get(i);
        std::stringstream ss;
        ss << "/NodeList/" << node->GetId() << "/$ns3::EnergySource/RemainingEnergy";
        Config::ConnectWithoutContext(ss.str(), MakeCallback(&
                                                            [](double oldValue, double newValue) {
                                                                NS_LOG_INFO("Node energy change: " << newValue);
                                                                if (newValue <= 0)
                                                                {
                                                                    NS_LOG_INFO("Node " << Simulator::GetContext() << " out of energy");
                                                                }
                                                            }));

    }

    // Stop simulation when all nodes run out of energy
    Simulator::Stop(Seconds(simulationTime + 1));

    // Run simulation
    Simulator::Run();

    // Cleanup
    Simulator::Destroy();

    return 0;
}