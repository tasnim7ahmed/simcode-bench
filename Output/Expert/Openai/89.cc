#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"
#include "ns3/basic-energy-harvester.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EnergyEfficientWirelessSimulation");

std::ofstream energyLog;

void
LogEnergy(double previousRxEnergy, Ptr<BasicEnergySource> energySource, Ptr<BasicEnergyHarvester> harvester, Time interval)
{
    double now = Simulator::Now().GetSeconds();
    double newEnergy = energySource->GetRemainingEnergy();
    double harvestedPower = harvester->GetHarvestedPower();
    energyLog << now << " " << newEnergy << " " << harvestedPower << std::endl;
    Simulator::Schedule(interval, &LogEnergy, newEnergy, energySource, harvester, interval);
}

int
main(int argc, char *argv[])
{
    Time::SetResolution(Time::NS);
    LogComponentEnable("EnergyEfficientWirelessSimulation", LOG_LEVEL_INFO);

    uint32_t numNodes = 2;
    double simTime = 10.0;

    NodeContainer nodes;
    nodes.Create(numNodes);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
    WifiMacHelper mac;
    Ssid ssid = Ssid("energy-wifi");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, nodes.Get(1));

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, nodes.Get(0));

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(NetDeviceContainer(apDevices, staDevices));

    // Energy Model
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1.0));
    EnergySourceContainer sources = energySourceHelper.Install(nodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
    radioEnergyHelper.Set("RxCurrentA", DoubleValue(0.0197));
    DeviceEnergyModelContainer deviceModels;
    deviceModels.Add(radioEnergyHelper.Install(apDevices.Get(0), sources.Get(0)));
    deviceModels.Add(radioEnergyHelper.Install(staDevices.Get(0), sources.Get(1)));

    // Energy Harvester
    Ptr<UniformRandomVariable> randomHarvester = CreateObject<UniformRandomVariable>();
    randomHarvester->SetAttribute("Min", DoubleValue(0.0));
    randomHarvester->SetAttribute("Max", DoubleValue(0.1));

    BasicEnergyHarvesterHelper harvesterHelper;
    harvesterHelper.Set("PeriodicHarvestedPowerUpdateInterval", TimeValue(Seconds(1.0)));

    // configure harvester for both nodes
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<BasicEnergySource> source = DynamicCast<BasicEnergySource>(sources.Get(i));
        harvesterHelper.Set("HarvestedPower", DoubleValue(randomHarvester->GetValue()));
        Ptr<BasicEnergyHarvester> harvester = harvesterHelper.Install(source).Get(0);
        // update harvested power randomly every second
        Simulator::ScheduleNow([harvester, randomHarvester]() {
            Simulator::Schedule(Seconds(1.0), [&]() {
                double harvestedPower = randomHarvester->GetValue();
                harvester->SetHarvestedPower(harvestedPower);
                Simulator::Schedule(Seconds(1.0), [&]() {
                    double nextHarvestedPower = randomHarvester->GetValue();
                    harvester->SetHarvestedPower(nextHarvestedPower);
                });
            });
        });
    }

    // Application: Sender sends to receiver every 1s for 10s
    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(nodes.Get(1));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    UdpClientHelper client(interfaces.GetAddress(1), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    // Calculate payload size for 0.0023s transmission time at 1 Mbps
    uint32_t payloadSize = static_cast<uint32_t>(1e6 * 0.0023 / 8); // 1 Mbps
    client.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApp = client.Install(nodes.Get(0));
    clientApp.Start(Seconds(0.0));
    clientApp.Stop(Seconds(simTime));

    // Logging for receiver node energy
    energyLog.open("receiver_energy.log");
    energyLog << "#Time(s) ResidualEnergy(J) HarvestedPower(W)\n";
    Ptr<BasicEnergySource> rxEnergySource = DynamicCast<BasicEnergySource>(sources.Get(1));
    Ptr<BasicEnergyHarvester> rxHarvester = rxEnergySource->GetObject<BasicEnergyHarvester>();
    Simulator::ScheduleNow(&LogEnergy, rxEnergySource->GetRemainingEnergy(), rxEnergySource, rxHarvester, Seconds(1.0));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    double initialEnergy = 1.0;
    double finalEnergy = rxEnergySource->GetRemainingEnergy();
    double consumedEnergy = initialEnergy - finalEnergy;
    std::cout << "==== Receiver Energy Statistics ====" << std::endl;
    std::cout << "Initial energy: " << initialEnergy << " J" << std::endl;
    std::cout << "Final residual energy: " << finalEnergy << " J" << std::endl;
    std::cout << "Total energy consumed: " << consumedEnergy << " J" << std::endl;
    std::cout << "Last harvested power: " << rxHarvester->GetHarvestedPower() << " W" << std::endl;

    energyLog.close();
    Simulator::Destroy();
    return 0;
}