#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/energy-module.h"

void RemainingEnergyTrace(uint32_t nodeId, ns3::Ptr<ns3::OutputStreamWrapper> stream, double remainingEnergy);
void EnergyStateChanged(uint32_t nodeId, ns3::Ptr<ns3::OutputStreamWrapper> stream, uint32_t oldState, uint32_t newState);

int main(int argc, char *argv[])
{
    double simulationTime = 100.0;
    std::string dataRate = "5Mbps";
    uint32_t packetSize = 1024;
    double txPower = 10.0;
    double initialEnergy = 100.0;
    double txCurrent = 0.5;
    double rxCurrent = 0.2;
    double idleCurrent = 0.01;
    double sleepCurrent = 0.001;
    double voltage = 3.7;

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds", simulationTime);
    cmd.AddValue("dataRate", "Data rate of the OnOff application", dataRate);
    cmd.AddValue("packetSize", "Packet size of the OnOff application (bytes)", packetSize);
    cmd.AddValue("txPower", "Transmission power in dBm", txPower);
    cmd.AddValue("initialEnergy", "Initial energy of the battery in Joules", initialEnergy);
    cmd.AddValue("txCurrent", "Transmit current in Amps", txCurrent);
    cmd.AddValue("rxCurrent", "Receive current in Amps", rxCurrent);
    cmd.AddValue("idleCurrent", "Idle current in Amps", idleCurrent);
    cmd.AddValue("sleepCurrent", "Sleep current in Amps", sleepCurrent);
    cmd.AddValue("voltage", "Nominal voltage of the battery in Volts", voltage);
    cmd.Parse(argc, argv);

    ns3::Time::SetResolution(ns3::NanoSeconds);

    ns3::NodeContainer nodes;
    nodes.Create(2);

    ns3::MobilityHelper mobility;
    ns3::Ptr<ns3::ListPositionAllocator> positionAlloc = ns3::CreateObject<ns3::ListPositionAllocator>();
    positionAlloc->Add(ns3::Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(ns3::Vector(10.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211b);

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::FriisPropagationLossModel");

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("TxPowerStart", ns3::DoubleValue(txPower));
    phy.Set("TxPowerEnd", ns3::DoubleValue(txPower));

    ns3::WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    ns3::NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    ns3::InternetStackHelper stack;
    stack.Install(nodes);

    ns3::Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = address.Assign(devices);

    ns3::EnergySourceHelper energySourceHelper;
    energySourceHelper.SetTypeId("ns3::RvBatteryEnergySource");
    energySourceHelper.Set("InitialEnergyJoules", ns3::DoubleValue(initialEnergy));
    energySourceHelper.Set("NominalVoltage_V", ns3::DoubleValue(voltage));

    ns3::DeviceEnergyHelper deviceEnergyHelper;
    deviceEnergyHelper.SetTypeId("ns3::WifiRadioEnergyModel");
    deviceEnergyHelper.Set("TxCurrentA", ns3::DoubleValue(txCurrent));
    deviceEnergyHelper.Set("RxCurrentA", ns3::DoubleValue(rxCurrent));
    deviceEnergyHelper.Set("IdleCurrentA", ns3::DoubleValue(idleCurrent));
    deviceEnergyHelper.Set("SleepCurrentA", ns3::DoubleValue(sleepCurrent));
    deviceEnergyHelper.Set("Voltage", ns3::DoubleValue(voltage));
    
    ns3::AsciiTraceHelper ascii;
    ns3::Ptr<ns3::OutputStreamWrapper> energyStateStream = ascii.CreateFileStream("energy-states.csv");
    *energyStateStream->GetStream() << "Time,NodeId,OldState,NewState" << std::endl;

    ns3::Ptr<ns3::OutputStreamWrapper> energyRemainingStream = ascii.CreateFileStream("energy-remaining.csv");
    *energyRemainingStream->GetStream() << "Time,NodeId,RemainingEnergyJoules" << std::endl;

    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        ns3::Ptr<ns3::NetDevice> netDevice = devices.Get(i);
        deviceEnergyHelper.Install(netDevice);
        ns3::Ptr<ns3::WifiNetDevice> wifiNetDevice = ns3::DynamicCast<ns3::WifiNetDevice>(netDevice);
        NS_ASSERT_MSG(wifiNetDevice, "Failed to cast NetDevice to WifiNetDevice");

        ns3::Ptr<ns3::DeviceEnergyModel> deviceEnergyModel = wifiNetDevice->GetDeviceEnergyModel();
        NS_ASSERT_MSG(deviceEnergyModel, "Failed to get DeviceEnergyModel");

        ns3::Ptr<ns3::EnergySource> energySource = deviceEnergyModel->GetEnergySource();
        NS_ASSERT_MSG(energySource, "Failed to get EnergySource");

        ns3::Ptr<ns3::RvBatteryEnergySource> battery = ns3::DynamicCast<ns3::RvBatteryEnergySource>(energySource);
        NS_ASSERT_MSG(battery, "Failed to cast EnergySource to RvBatteryEnergySource");

        uint32_t nodeId = nodes.Get(i)->GetId();
        
        battery->TraceConnectWithoutContext("EnergyStateChanged", ns3::MakeBoundCallback(&EnergyStateChanged, nodeId, energyStateStream));
        battery->TraceConnectWithoutContext("RemainingEnergy", ns3::MakeBoundCallback(&RemainingEnergyTrace, nodeId, energyRemainingStream));
    }

    uint16_t port = 9;
    ns3::OnOffHelper onoff("ns3::UdpSocketFactory", ns3::InetSocketAddress(interfaces.GetAddress(1), port));
    onoff.SetAttribute("OnTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", ns3::StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", ns3::DataRateValue(ns3::DataRate(dataRate)));

    ns3::ApplicationContainer clientApps = onoff.Install(nodes.Get(0));
    clientApps.Start(ns3::Seconds(1.0));
    clientApps.Stop(ns3::Seconds(simulationTime - 1.0));

    ns3::PacketSinkHelper sink("ns3::UdpSocketFactory", ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer serverApps = sink.Install(nodes.Get(1));
    serverApps.Start(ns3::Seconds(0.0));
    serverApps.Stop(ns3::Seconds(simulationTime));

    phy.EnablePcapAll("wifi-energy-adhoc");

    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}

void RemainingEnergyTrace(uint32_t nodeId, ns3::Ptr<ns3::OutputStreamWrapper> stream, double remainingEnergy)
{
    *stream->GetStream() << ns3::Simulator::Now().GetSeconds() << ","
                         << nodeId << ","
                         << remainingEnergy << std::endl;
}

void EnergyStateChanged(uint32_t nodeId, ns3::Ptr<ns3::OutputStreamWrapper> stream, uint32_t oldState, uint32_t newState)
{
    std::string oldStateStr;
    std::string newStateStr;

    switch (oldState)
    {
    case ns3::EnergySource::EnergyState::ENERGY_IDLE: oldStateStr = "IDLE"; break;
    case ns3::EnergySource::EnergyState::ENERGY_TX: oldStateStr = "TX"; break;
    case ns3::EnergySource::EnergyState::ENERGY_RX: oldStateStr = "RX"; break;
    case ns3::EnergySource::EnergyState::ENERGY_SLEEP: oldStateStr = "SLEEP"; break;
    case ns3::EnergySource::EnergyState::ENERGY_OFF: oldStateStr = "OFF"; break;
    default: oldStateStr = "UNKNOWN"; break;
    }

    switch (newState)
    {
    case ns3::EnergySource::EnergyState::ENERGY_IDLE: newStateStr = "IDLE"; break;
    case ns3::EnergySource::EnergyState::ENERGY_TX: newStateStr = "TX"; break;
    case ns3::EnergySource::EnergyState::ENERGY_RX: newStateStr = "RX"; break;
    case ns3::EnergySource::EnergyState::ENERGY_SLEEP: newStateStr = "SLEEP"; break;
    case ns3::EnergySource::EnergyState::ENERGY_OFF: newStateStr = "OFF"; break;
    default: newStateStr = "UNKNOWN"; break;
    }

    *stream->GetStream() << ns3::Simulator::Now().GetSeconds() << ","
                         << nodeId << ","
                         << oldStateStr << "," << newStateStr << std::endl;

    if (newState == ns3::EnergySource::EnergyState::ENERGY_SLEEP)
    {
        std::cout << "Node " << nodeId << " entered SLEEP state due to energy depletion at " << ns3::Simulator::Now().GetSeconds() << "s." << std::endl;
    }
}