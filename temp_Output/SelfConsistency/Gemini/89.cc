#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include "ns3/netanim-module.h"
#include "ns3/random-variable-stream.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyHarvesterSimulation");

int main(int argc, char* argv[]) {
    // Enable logging
    LogComponentEnable("EnergyHarvesterSimulation", LOG_LEVEL_INFO);

    // Set simulation time
    Time::SetResolution(Time::NS);
    double simulationTime = 10.0;

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure WiFi channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    // Configure WiFi MAC
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    NqosWifiMacHelper mac = NqosWifiMacHelper::Default();
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetSSID(ssid);
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(1)); // Station (receiver)

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, nodes.Get(0)); // Access Point (sender)

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(apDevices);
    interfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Configure UDP client (sender)
    uint16_t port = 9;
    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = client.Install(nodes.Get(0));
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime + 1));

    // Configure UDP server (receiver)
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(nodes.Get(1));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime + 1));

    // Enable energy model
    BasicEnergySourceHelper basicSourceHelper;
    EnergySourceContainer sources = basicSourceHelper.Install(nodes);
    basicSourceHelper.SetInitialEnergy(1); // Joules

    WifiRadioEnergyModelHelper radioEnergyModelHelper;
    radioEnergyModelHelper.SetTxCurrentA(0.0174);
    radioEnergyModelHelper.SetRxCurrentA(0.0197);
    EnergySourceContainer deviceSources = radioEnergyModelHelper.Install(staDevices, sources);

    // Configure energy harvester (at receiver)
    BasicEnergyHarvesterHelper basicHarvesterHelper;
    basicHarvesterHelper.Set("HarvestablePower", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=0.1]"));
    basicHarvesterHelper.Install(sources.Get(1)); // Connect to the receiver's energy source

    // Tracing
    Ptr<BasicEnergySource> recvEnergysource = DynamicCast<BasicEnergySource>(sources.Get(1));
    std::ofstream os;
    os.open("energy-harvesting.dat");
    os << "Time\tCurrent\tVoltage\tRx\tTx\tIdle\tSleep\tTotal\tHarvested\tResidual\n";

    Simulator::Schedule(Seconds(0.1), [&]() {
        os << Simulator::Now().GetSeconds() << "\t"
           << recvEnergysource->GetCurrentA() << "\t"
           << recvEnergysource->GetVoltage() << "\t"
           << recvEnergysource->GetRxConsumption() << "\t"
           << recvEnergysource->GetTxConsumption() << "\t"
           << recvEnergysource->GetIdleConsumption() << "\t"
           << recvEnergysource->GetSleepConsumption() << "\t"
           << recvEnergysource->GetTotalEnergyConsumption() << "\t"
           << recvEnergysource->GetTotalEnergyHarvested() << "\t"
           << recvEnergysource->GetRemainingEnergy() << "\n";
    });

    for (double i = 1.0; i <= simulationTime; i += 1.0) {
        Simulator::Schedule(Seconds(i), [&]() {
            os << Simulator::Now().GetSeconds() << "\t"
               << recvEnergysource->GetCurrentA() << "\t"
               << recvEnergysource->GetVoltage() << "\t"
               << recvEnergysource->GetRxConsumption() << "\t"
               << recvEnergysource->GetTxConsumption() << "\t"
               << recvEnergysource->GetIdleConsumption() << "\t"
               << recvEnergysource->GetSleepConsumption() << "\t"
               << recvEnergysource->GetTotalEnergyConsumption() << "\t"
               << recvEnergysource->GetTotalEnergyHarvested() << "\t"
               << recvEnergysource->GetRemainingEnergy() << "\n";
        });
    }

    // Run simulation
    Simulator::Stop(Seconds(simulationTime + 1));
    Simulator::Run();

    // Print energy statistics (at the end of the simulation)
    std::cout << "Remaining energy at node 1: " << recvEnergysource->GetRemainingEnergy() << " Joules\n";

    os.close();
    Simulator::Destroy();
    return 0;
}