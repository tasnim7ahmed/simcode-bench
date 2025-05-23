#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/applications-module.h"
#include "ns3/ssid-helper.h"

#include <iostream>
#include <random>

using namespace ns3;

Ptr<BasicEnergySource> g_receiverEnergySource;
Ptr<BasicEnergyHarvester> g_receiverHarvester;
Ptr<WifiRadioEnergyModel> g_receiverWifiRadioEnergyModel;

static std::default_random_engine g_generator;
static std::uniform_real_distribution<double> g_distribution(0.0, 0.1);

void UpdateHarvestPower(Ptr<BasicEnergyHarvester> harvester)
{
    double randomPower = g_distribution(g_generator);
    harvester->SetHarvestPower(randomPower);
    Simulator::Schedule(Seconds(1.0), &UpdateHarvestPower, harvester);
}

void LogEnergyStatistics()
{
    if (g_receiverEnergySource == nullptr || g_receiverHarvester == nullptr || g_receiverWifiRadioEnergyModel == nullptr)
    {
        return;
    }

    Time now = Simulator::Now();
    double residualEnergy = g_receiverEnergySource->GetRemainingEnergy();
    double harvestedPower = g_receiverHarvester->GetHarvestPower();
    double totalConsumedEnergy = g_receiverWifiRadioEnergyModel->GetTotalEnergyConsumption();

    std::cout << "Time: " << now.GetSeconds()
              << "s, Residual Energy (Receiver): " << residualEnergy
              << "J, Harvester Power (Receiver): " << harvestedPower
              << "W, Total Wifi Consumed Energy (Receiver): " << totalConsumedEnergy
              << "J" << std::endl;

    Simulator::Schedule(Seconds(1.0), &LogEnergyStatistics);
}


int main(int argc, char *argv[])
{
    Time::SetResolution(NanoSeconds(1));
    RngSeedManager::SetSeed(1);
    g_generator.seed(RngSeedManager::GetSeed());

    double simulationTime = 10.0;
    double initialEnergy = 1.0;
    double txCurrent = 0.0174;
    double rxCurrent = 0.0197;
    double wifiVoltage = 3.7;
    
    // Packet size: 1024 bytes
    // Desired Tx time: 0.0023 seconds
    // Required Data Rate = (1024 bytes * 8 bits/byte) / 0.0023 s = 8192 bits / 0.0023 s = 3561739.13 bps
    DataRate packetDataRate ("3.56Mbps"); 
    uint32_t packetSize = 1024; 
    
    // Calculate OnTime and OffTime for OnOffHelper to send one packet every second
    // OnTime is roughly the time to transmit one packet
    // OffTime is 1 second - OnTime
    double onTime = 0.0023;
    double offTime = 1.0 - onTime;

    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::AarfWifiManager");

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    Ptr<YansWifiChannel> channel = wifiChannel.Create();
    wifiPhy.SetChannel(channel);
    wifiPhy.Set("TxPowerStart", DoubleValue(0.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(0.0));

    NistWifiMacHelper wifiMac;
    Ssid ssid = Ssid("ns3-energy");

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(NanoSeconds(102400)));
    NetDeviceContainer apDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(0));

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices = wifi.Install(wifiPhy, wifiMac, nodes.Get(1));

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apIpIfaces = ipv4.Assign(apDevices);
    Ipv4InterfaceContainer staIpIfaces = ipv4.Assign(staDevices);

    Ptr<BasicEnergySource> senderEnergySource = CreateObject<BasicEnergySource>();
    senderEnergySource->SetInitialEnergy(initialEnergy);
    senderEnergySource->SetEnergyVoltage(wifiVoltage);

    Ptr<BasicEnergyHarvester> senderHarvester = CreateObject<BasicEnergyHarvester>();
    senderEnergySource->AddEnergyHarvester(senderHarvester);

    Ptr<WifiRadioEnergyModel> senderRadioEnergyModel = CreateObject<WifiRadioEnergyModel>();
    senderRadioEnergyModel->SetTxCurrentA(txCurrent);
    senderRadioEnergyModel->SetRxCurrentA(rxCurrent);
    senderRadioEnergyModel->SetSleepCurrentA(0.0001);
    senderRadioEnergyModel->AssignEnergySource(senderEnergySource);

    Ptr<WifiNetDevice> senderWifiNetDevice = DynamicCast<WifiNetDevice>(apDevices.Get(0));
    senderWifiNetDevice->AggregateObject(senderRadioEnergyModel);
    senderEnergySource->UpdateEnergyAccount(senderRadioEnergyModel);

    Ptr<BasicEnergySource> receiverEnergySource = CreateObject<BasicEnergySource>();
    receiverEnergySource->SetInitialEnergy(initialEnergy);
    receiverEnergySource->SetEnergyVoltage(wifiVoltage);

    Ptr<BasicEnergyHarvester> receiverHarvester = CreateObject<BasicEnergyHarvester>();
    receiverEnergySource->AddEnergyHarvester(receiverHarvester);

    Ptr<WifiRadioEnergyModel> receiverRadioEnergyModel = CreateObject<WifiRadioEnergyModel>();
    receiverRadioEnergyModel->SetTxCurrentA(txCurrent);
    receiverRadioEnergyModel->SetRxCurrentA(rxCurrent);
    receiverRadioEnergyModel->SetSleepCurrentA(0.0001);
    receiverRadioEnergyModel->AssignEnergySource(receiverEnergySource);

    Ptr<WifiNetDevice> receiverWifiNetDevice = DynamicCast<WifiNetDevice>(staDevices.Get(0));
    receiverWifiNetDevice->AggregateObject(receiverRadioEnergyModel);
    receiverEnergySource->UpdateEnergyAccount(receiverRadioEnergyModel);

    g_receiverEnergySource = receiverEnergySource;
    g_receiverHarvester = receiverHarvester;
    g_receiverWifiRadioEnergyModel = receiverRadioEnergyModel;

    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(staIpIfaces.GetAddress(0), 9));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(onTime) + "]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(offTime) + "]"));
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onoff.SetAttribute("DataRate", DataRateValue(packetDataRate));

    ApplicationContainer senderApps = onoff.Install(nodes.Get(0));
    senderApps.Start(Seconds(1.0));
    senderApps.Stop(Seconds(simulationTime));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer receiverApps = sink.Install(nodes.Get(1));
    receiverApps.Start(Seconds(0.0));
    receiverApps.Stop(Seconds(simulationTime + 1.0));

    Simulator::Schedule(Seconds(0.0), &UpdateHarvestPower, senderHarvester);
    Simulator::Schedule(Seconds(0.0), &UpdateHarvestPower, receiverHarvester);
    Simulator::Schedule(Seconds(0.0), &LogEnergyStatistics);

    Simulator::Stop(Seconds(simulationTime + 1.0));
    Simulator::Run();

    std::cout << "\n--- Simulation Completed ---" << std::endl;
    std::cout << "Final Residual Energy (Receiver): " << g_receiverEnergySource->GetRemainingEnergy() << "J" << std::endl;
    std::cout << "Final Total Wifi Consumed Energy (Receiver): " << g_receiverWifiRadioEnergyModel->GetTotalEnergyConsumption() << "J" << std::endl;

    Simulator::Destroy();

    return 0;
}