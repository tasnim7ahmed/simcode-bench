#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularV2VNetAnimExample");

int main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);

    uint32_t nVehicles = 4;
    double simulationTime = 20.0; // seconds
    double distance = 50.0; // meters between vehicles
    double speed = 15.0; // m/s

    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer vehicles;
    vehicles.Create(nVehicles);

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_PHY_STANDARD_80211p);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer vehicleDevices = wifiHelper.Install(wifiPhy, wifiMac, vehicles);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        positionAlloc->Add(Vector(i * distance, 0.0, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(vehicles);

    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        Ptr<ConstantVelocityMobilityModel> m = vehicles.Get(i)->GetObject<ConstantVelocityMobilityModel>();
        m->SetVelocity(Vector(speed, 0.0, 0.0));
    }

    InternetStackHelper internet;
    internet.Install(vehicles);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(vehicleDevices);

    uint16_t port = 4000;
    double interval = 1.0; // seconds
    uint32_t packetSize = 256; // bytes
    uint32_t maxPackets = uint32_t(simulationTime / interval);

    std::vector<ApplicationContainer> serverApps(nVehicles);
    std::vector<ApplicationContainer> clientApps(nVehicles);

    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        UdpServerHelper server(port + i);
        serverApps[i] = server.Install(vehicles.Get(i));
        serverApps[i].Start(Seconds(0.0));
        serverApps[i].Stop(Seconds(simulationTime + 1));

        for (uint32_t j = 0; j < nVehicles; ++j)
        {
            if (i == j)
                continue;
            UdpClientHelper client(interfaces.GetAddress(j), port + j);
            client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
            client.SetAttribute("Interval", TimeValue(Seconds(interval)));
            client.SetAttribute("PacketSize", UintegerValue(packetSize));
            ApplicationContainer app = client.Install(vehicles.Get(i));
            app.Start(Seconds(1.0));
            app.Stop(Seconds(simulationTime));
            clientApps[i].Add(app);
        }
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AnimationInterface anim("vehicular-v2v.xml");
    for (uint32_t i = 0; i < nVehicles; ++i)
    {
        anim.UpdateNodeDescription(vehicles.Get(i), "Vehicle-" + std::to_string(i + 1));
        anim.UpdateNodeColor(vehicles.Get(i), 255, 0, 0);
    }
    anim.SetMaxPktsPerTraceFile(100000);

    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    double totalTxPackets = 0;
    double totalRxPackets = 0;
    double totalDelay = 0;
    double totalRx = 0;

    for (auto const &flow : stats)
    {
        totalTxPackets += flow.second.txPackets;
        totalRxPackets += flow.second.rxPackets;
        totalRx += flow.second.rxBytes;
        if (flow.second.rxPackets > 0)
        {
            totalDelay += flow.second.delaySum.GetSeconds();
        }
    }

    std::cout << "Total Transmitted Packets: " << totalTxPackets << std::endl;
    std::cout << "Total Received Packets:    " << totalRxPackets << std::endl;
    std::cout << "Packet Loss:               " << (totalTxPackets - totalRxPackets) << std::endl;
    if (totalRxPackets > 0)
    {
        std::cout << "Average Delay (s):         " << totalDelay / totalRxPackets << std::endl;
        std::cout << "Average Throughput (kbps): " << (totalRx * 8.0 / (simulationTime * 1000.0)) << std::endl;
    }

    Simulator::Destroy();
    return 0;
}