#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularNetworkSimulation");

void ReceivePacket(Ptr<const Packet> packet, const Address &addr)
{
    // Packet received callback (can be expanded for logging if necessary)
}

int main(int argc, char *argv[])
{
    uint32_t numVehicles = 4;
    double simulationTime = 20.0; // seconds
    uint16_t port = 4000;

    // Enable logging if needed
    //LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    //LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create node container
    NodeContainer vehicles;
    vehicles.Create(numVehicles);

    // Configure WiFi (802.11p for VANET)
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());

    Wifi80211pHelper wifi;
    QosWaveMacHelper mac = QosWaveMacHelper::Default();

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6MbpsBW10MHz"), "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));
    NetDeviceContainer devices = wifi.Install(phy, mac, vehicles);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(vehicles);

    // Assign IP addresses
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Mobility: Place vehicles along a straight line and assign constant velocity
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    double distance = 10.0; // meters between vehicles

    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        positionAlloc->Add(Vector(i * distance, 0.0, 0.0));
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        Ptr<ConstantVelocityMobilityModel> cvmm = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        cvmm->SetVelocity(Vector(10.0, 0.0, 0.0)); // 10 m/s along X axis
    }

    // Install UDP Server applications on each vehicle
    ApplicationContainer serverApps;
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        UdpServerHelper server(port);
        ApplicationContainer serverApp = server.Install(vehicles.Get(i));
        serverApp.Start(Seconds(0.5));
        serverApp.Stop(Seconds(simulationTime));
        serverApps.Add(serverApp);
    }

    // Install UDP Client applications (each vehicle sends packets to all others, excluding self)
    double interval = 1.0; // one packet per second
    uint32_t maxPackets = 100;
    uint32_t packetSize = 256;

    ApplicationContainer clientApps;
    for (uint32_t src = 0; src < numVehicles; ++src)
    {
        for (uint32_t dst = 0; dst < numVehicles; ++dst)
        {
            if (src != dst)
            {
                UdpClientHelper client(interfaces.GetAddress(dst), port);
                client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
                client.SetAttribute("Interval", TimeValue(Seconds(interval)));
                client.SetAttribute("PacketSize", UintegerValue(packetSize));
                ApplicationContainer clientApp = client.Install(vehicles.Get(src));
                clientApp.Start(Seconds(1.0));
                clientApp.Stop(Seconds(simulationTime));
                clientApps.Add(clientApp);
            }
        }
    }

    // FlowMonitor to collect packet loss and delay
    Ptr<FlowMonitor> flowmon;
    FlowMonitorHelper flowmonHelper;
    flowmon = flowmonHelper.InstallAll();

    // NetAnim
    AnimationInterface anim("vehicular-netanim.xml");
    for (uint32_t i = 0; i < numVehicles; ++i)
    {
        anim.UpdateNodeDescription(vehicles.Get(i), "Vehicle" + std::to_string(i + 1));
        anim.UpdateNodeColor(vehicles.Get(i), 255, 0, 0); // Red color
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    // Results: Packet loss and delay
    flowmon->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowmon->GetFlowStats();

    std::cout << "FlowID, SourceIP, DestIP, TxPackets, RxPackets, LostPackets, Throughput(bps), Delay(ms)" << std::endl;
    for (auto const &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        std::cout << flow.first << ", "
                  << t.sourceAddress << ", "
                  << t.destinationAddress << ", "
                  << flow.second.txPackets << ", "
                  << flow.second.rxPackets << ", "
                  << flow.second.lostPackets << ", ";
        if (flow.second.timeLastRxPacket.GetSeconds() > 0)
        {
            double throughput = (flow.second.rxBytes * 8.0) / (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds());
            std::cout << throughput << ", ";
        }
        else
        {
            std::cout << "0, ";
        }
        if (flow.second.rxPackets > 0)
        {
            std::cout << (flow.second.delaySum.GetMilliSeconds() / flow.second.rxPackets) << std::endl;
        }
        else
        {
            std::cout << "0" << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}