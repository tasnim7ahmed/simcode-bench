#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-module.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"

#include <iostream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WSNEnergyDepletion");

// Global variable to keep track of active sensor nodes
static uint32_t g_activeSensorNodes = 0;

// Callback function for energy depletion
void
EnergyDepletionCallback(Ptr<RvBatteryModel> battery)
{
    NS_LOG_INFO("Node " << battery->GetNode()->GetId() << " energy depleted at " << Simulator::Now().GetSeconds() << "s.");
    g_activeSensorNodes--;

    if (g_activeSensorNodes == 0)
    {
        NS_LOG_INFO("All sensor nodes depleted energy. Stopping simulation.");
        Simulator::Stop();
    }
}

int
main(int argc, char* argv[])
{
    // 1. Set up simulation parameters
    uint32_t gridRows = 5;
    uint32_t gridCols = 5;
    double nodeSpacing = 50.0; // Meters
    double initialEnergyJ = 100.0; // Joules
    double txCurrentA = 0.170; // Amps (e.g., for transmit mode, typical for Wifi)
    double rxCurrentA = 0.080; // Amps (e.g., for receive mode, typical for Wifi)
    double idleCurrentA = 0.005; // Amps (e.g., for idle mode)
    double sleepCurrentA = 0.001; // Amps (e.g., for sleep mode)
    double voltageV = 3.3; // Volts

    uint32_t packetSize = 1024; // Bytes
    Time interPacketInterval = Seconds(1.0); // Time between packets
    uint32_t maxPackets = 0; // 0 for unlimited packets until energy depletion or sim time

    double sinkX = (gridCols - 1) * nodeSpacing / 2.0;
    double sinkY = (gridRows - 1) * nodeSpacing / 2.0 + nodeSpacing * 2; // Place sink above the grid
    double simStopTime = 600.0; // seconds, maximum simulation time

    bool enableFlowMonitor = true; // Enable FlowMonitor for statistics

    CommandLine cmd(__FILE__);
    cmd.AddValue("gridRows", "Number of rows in the sensor grid", gridRows);
    cmd.AddValue("gridCols", "Number of columns in the sensor grid", gridCols);
    cmd.AddValue("nodeSpacing", "Distance between sensor nodes in meters", nodeSpacing);
    cmd.AddValue("initialEnergyJ", "Initial energy for sensor nodes in Joules", initialEnergyJ);
    cmd.AddValue("txCurrentA", "Transmit current in Amps", txCurrentA);
    cmd.AddValue("rxCurrentA", "Receive current in Amps", rxCurrentA);
    cmd.AddValue("idleCurrentA", "Idle current in Amps", idleCurrentA);
    cmd.AddValue("sleepCurrentA", "Sleep current in Amps", sleepCurrentA);
    cmd.AddValue("packetSize", "Size of data packets in bytes", packetSize);
    cmd.AddValue("interPacketInterval", "Time between packet transmissions", interPacketInterval.GetSeconds());
    cmd.AddValue("maxPackets", "Maximum packets to send (0 for unlimited)", maxPackets);
    cmd.AddValue("sinkX", "X-coordinate of the sink node", sinkX);
    cmd.AddValue("sinkY", "Y-coordinate of the sink node", sinkY);
    cmd.AddValue("simStopTime", "Maximum simulation stop time in seconds", simStopTime);
    cmd.AddValue("enableFlowMonitor", "Enable FlowMonitor", enableFlowMonitor);
    cmd.Parse(argc, argv);

    // Ensure parameters are positive
    NS_ASSERT_MSG(gridRows > 0 && gridCols > 0, "Grid dimensions must be positive.");
    NS_ASSERT_MSG(nodeSpacing > 0, "Node spacing must be positive.");
    NS_ASSERT_MSG(initialEnergyJ > 0, "Initial energy must be positive.");
    NS_ASSERT_MSG(txCurrentA >= 0 && rxCurrentA >= 0 && idleCurrentA >= 0 && sleepCurrentA >= 0, "Currents must be non-negative.");
    NS_ASSERT_MSG(voltageV > 0, "Voltage must be positive.");
    NS_ASSERT_MSG(packetSize > 0, "Packet size must be positive.");
    NS_ASSERT_MSG(interPacketInterval.GetSeconds() > 0, "Inter-packet interval must be positive.");

    // Enable logging
    LogComponentEnable("WSNEnergyDepletion", LOG_LEVEL_INFO);
    LogComponentEnable("RvBatteryModel", LOG_LEVEL_DEBUG);
    LogComponentEnable("WifiRadioEnergyModel", LOG_LEVEL_DEBUG);
    LogComponentEnable("NodeEnergyModel", LOG_LEVEL_DEBUG);
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_DEBUG);
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

    // 2. Create nodes
    NodeContainer sensorNodes;
    sensorNodes.Create(gridRows * gridCols);
    NodeContainer sinkNode;
    sinkNode.Create(1);

    NodeContainer allNodes = NodeContainer(sensorNodes, sinkNode);

    // 3. Install Mobility
    MobilityHelper mobility;
    Ptr<GridPositionAllocator> positionAlloc = CreateObject<GridPositionAllocator>();
    positionAlloc->SetResolution(nodeSpacing);
    positionAlloc->SetDimensions(gridCols, gridRows);
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    Ptr<ConstantPositionMobilityModel> sinkMobility = CreateObject<ConstantPositionMobilityModel>();
    sinkMobility->SetPosition(Vector(sinkX, sinkY, 0.0));
    sinkNode.Get(0)->AggregateObject(sinkMobility);

    // 4. Install Internet Stack (AODV)
    AodvHelper aodv;
    InternetStackHelper internet;
    internet.SetRoutingHelper(aodv);
    internet.Install(allNodes);

    // 5. Install Wifi Devices
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b); // A common standard for WSN or older Wifi

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel"); // Simple propagation model for WSN
    Ptr<YansWifiChannel> wifiChannel = channel.Create();

    YansWifiPhyHelper phy;
    phy.SetChannel(wifiChannel);
    // Set a lower transmit power appropriate for WSNs (e.g., 0dBm for 1mW)
    phy.Set("TxPowerStart", DoubleValue(0.0));
    phy.Set("TxPowerEnd", DoubleValue(0.0));
    phy.Set("TxPowerLevels", UintegerValue(1));

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac", "Ssid", SsidValue(Ssid("WSN_Adhoc")));

    NetDeviceContainer devices = wifi.Install(phy, mac, allNodes);

    // 6. Install Energy Model for sensor nodes
    // Keep track of battery models to potentially print final energy
    std::vector<Ptr<RvBatteryModel>> sensorBatteries;
    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
    {
        Ptr<Node> node = sensorNodes.Get(i);

        // Configure Battery Model
        Ptr<RvBatteryModel> battery = CreateObject<RvBatteryModel>();
        battery->SetEnergySourceInitialEnergy(initialEnergyJ);
        // Link battery to NodeEnergyModel through aggregation
        node->AggregateObject(battery);
        sensorBatteries.push_back(battery);

        // Set depletion callback
        battery->SetEnergyDepletionCallback(MakeCallback(&EnergyDepletionCallback));
        g_activeSensorNodes++; // Increment active node counter

        // Configure Wifi Radio Energy Model
        Ptr<WifiRadioEnergyModel> radioEnergyModel = CreateObject<WifiRadioEnergyModel>();
        radioEnergyModel->SetEnergySource(battery);
        radioEnergyModel->SetTxCurrentA(txCurrentA);
        radioEnergyModel->SetRxCurrentA(rxCurrentA);
        radioEnergyModel->SetIdleCurrentA(idleCurrentA);
        radioEnergyModel->SetSleepCurrentA(sleepCurrentA);
        radioEnergyModel->SetVoltageV(voltageV);

        // Link WifiRadioEnergyModel to the WifiNetDevice
        Ptr<WifiNetDevice> wifiDevice = DynamicCast<WifiNetDevice>(devices.Get(i));
        NS_ASSERT_MSG(wifiDevice != nullptr, "Could not get WifiNetDevice for sensor node.");
        wifiDevice->AggregateObject(radioEnergyModel);

        NS_LOG_DEBUG("Node " << node->GetId() << " (Sensor) energy model installed.");
    }

    // Assign IP Addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Get sink IP address. Sink is the last node in 'devices' container.
    Ipv4Address sinkIp = interfaces.GetAddress(sensorNodes.GetN()); 
    NS_LOG_INFO("Sink IP Address: " << sinkIp);

    // 7. Create Applications
    // Sink Application
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApps = packetSinkHelper.Install(sinkNode);
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simStopTime + 1.0)); // Stop slightly after sim end time

    // Sensor Applications
    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
    {
        UdpClientHelper udpClientHelper(sinkIp, 9);
        udpClientHelper.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        udpClientHelper.SetAttribute("Interval", TimeValue(interPacketInterval));
        udpClientHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer clientApps = udpClientHelper.Install(sensorNodes.Get(i));
        clientApps.Start(Seconds(1.0)); // Start client apps after sink
        clientApps.Stop(Seconds(simStopTime));
        NS_LOG_DEBUG("Sensor node " << sensorNodes.Get(i)->GetId() << " (IP: " << interfaces.GetAddress(i) << ") client app installed.");
    }

    // 8. Tracing (Optional)
    // phy.EnablePcap("wsn-energy", devices);
    // wifi.EnableAsciiTracing("wsn-energy", devices);

    if (enableFlowMonitor)
    {
#ifdef NS3_FLOW_MONITOR
        FlowMonitorHelper flowMonitorHelper;
        Ptr<FlowMonitor> flowMonitor = flowMonitorHelper.Install(allNodes);
#endif
    }

    // 9. Run simulation
    Simulator::Stop(Seconds(simStopTime));
    NS_LOG_INFO("Starting simulation for " << simStopTime << " seconds or until all nodes deplete energy.");
    Simulator::Run();
    NS_LOG_INFO("Simulation finished at " << Simulator::Now().GetSeconds() << " seconds.");

    // 10. Output statistics
    uint32_t totalPacketsReceived = DynamicCast<PacketSink>(sinkApps.Get(0))->GetTotalReceivedBytes() / packetSize;
    NS_LOG_INFO("Total packets received by sink: " << totalPacketsReceived);

    NS_LOG_INFO("Final Energy Levels:");
    for (uint32_t i = 0; i < sensorNodes.GetN(); ++i)
    {
        Ptr<RvBatteryModel> battery = sensorBatteries.at(i);
        NS_LOG_INFO("  Sensor Node " << sensorNodes.Get(i)->GetId() << ": " << battery->GetCurrentEnergyRemaining() << " J");
    }

    if (enableFlowMonitor)
    {
#ifdef NS3_FLOW_MONITOR
        flowMonitor->CheckFor = FlowMonitor::PACKET_LEVEL_STATS;
        flowMonitor->SerializeToXmlFile("wsn-energy.flowmon", true, true);
        NS_LOG_INFO("Flow monitor data saved to wsn-energy.flowmon");
#endif
    }

    Simulator::Destroy();
    return 0;
}