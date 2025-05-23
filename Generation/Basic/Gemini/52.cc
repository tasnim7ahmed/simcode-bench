#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    double simulationTime = 10.0; // seconds
    uint32_t packetSize = 1472; // bytes (UDP payload, max for 1500 byte IP packet)
    DataRate dataRate ("10Mbps"); // Client sending rate
    
    // Default Wi-Fi timing parameters for 802.11n 2.4GHz (HT-OFDM)
    // ns-3's default for DcfManager attributes are usually based on 802.11a/g (OFDM) or 802.11b (DSSS)
    // For 802.11n (HT), these values are typically:
    // Slot Time: 9 us
    // SIFS: 10 us
    // PIFS: SIFS + SlotTime = 19 us
    uint32_t slotTimeUs = 9;
    uint32_t sifsTimeUs = 10;
    uint32_t pifsTimeUs = 19; // SIFS + SlotTime

    CommandLine cmd (__FILE__);
    cmd.AddValue ("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue ("packetSize", "UDP client packet size in bytes", packetSize);
    cmd.AddValue ("dataRate", "UDP client application data rate (e.g., 10Mbps)", dataRate);
    cmd.AddValue ("slotTimeUs", "Wi-Fi Slot Time in microseconds", slotTimeUs);
    cmd.AddValue ("sifsTimeUs", "Wi-Fi SIFS Time in microseconds", sifsTimeUs);
    cmd.AddValue ("pifsTimeUs", "Wi-Fi PIFS Time in microseconds (SIFS + SlotTime usually)", pifsTimeUs);
    cmd.Parse (argc, argv);

    // Configure Wi-Fi timing parameters globally
    Config::SetDefault ("ns3::DcfManager::SlotTime", TimeValue (MicroSeconds (slotTimeUs)));
    Config::SetDefault ("ns3::DcfManager::SifsTime", TimeValue (MicroSeconds (sifsTimeUs)));
    Config::SetDefault ("ns3::DcfManager::PifsTime", TimeValue (MicroSeconds (pifsTimeUs)));

    NodeContainer wifiNodes;
    wifiNodes.Create (2); // 0: AP, 1: STA

    Ptr<Node> apNode = wifiNodes.Get (0);
    Ptr<Node> staNode = wifiNodes.Get (1);

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
    Ptr<YansWifiChannel> wifiChannel = channel.Create ();

    YansWifiPhyHelper phy;
    phy.SetChannel (wifiChannel);
    phy.Set ("TxPowerStart", DoubleValue (16.0));
    phy.Set ("TxPowerEnd", DoubleValue (16.0));
    phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiHelper wifi;
    wifi.SetStandard (WIFI_PHY_STANDARD_80211n_2_4GHZ);

    ApWifiMacHelper apWifiMac;
    apWifiMac.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (Ssid ("ns3-80211n")),
                       "BeaconInterval", TimeValue (MicroSeconds (102400)));

    StaWifiMacHelper staWifiMac;
    staWifiMac.SetType ("ns3::StaWifiMac",
                        "Ssid", SsidValue (Ssid ("ns3-80211n")),
                        "ActiveProbing", BooleanValue (false));

    NetDeviceContainer apDevice;
    apDevice = wifi.Install (phy, apWifiMac, apNode);

    NetDeviceContainer staDevice;
    staDevice = wifi.Install (phy, staWifiMac, staNode);

    MobilityHelper mobility;
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

    mobility.Install (apNode);
    apNode->GetObject<ConstantPositionMobilityModel> ()->SetPosition (Vector (0.0, 0.0, 0.0));

    mobility.Install (staNode);
    staNode->GetObject<ConstantPositionMobilityModel> ()->SetPosition (Vector (1.0, 0.0, 0.0));

    InternetStackHelper stack;
    stack.Install (wifiNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces = address.Assign (apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign (staDevice);

    uint16_t port = 9;

    UdpServerHelper server (port);
    ApplicationContainer serverApps = server.Install (staNode);
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (simulationTime + 1.0));

    UdpClientHelper client (staInterfaces.GetAddress (0), port);
    client.SetAttribute ("MaxBytes", UintegerValue (0));
    client.SetAttribute ("PacketSize", UintegerValue (packetSize));
    client.SetAttribute ("DataRate", DataRateValue (dataRate));

    ApplicationContainer clientApps = client.Install (apNode);
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (simulationTime));

    phy.EnablePcap ("wifi-timing-parameters", apDevice.Get (0));
    phy.EnablePcap ("wifi-timing-parameters", staDevice.Get (0));

    Simulator::Stop (Seconds (simulationTime + 2.0));
    Simulator::Run ();

    Ptr<UdpServer> serverApp = DynamicCast<UdpServer> (serverApps.Get (0));
    uint64_t totalRxBytes = serverApp->GetTotalRx ();
    double actualSimulationTime = simulationTime - 1.0;

    double throughputMbps = (totalRxBytes * 8.0) / (actualSimulationTime * 1000000.0);

    std::cout << "Configured Wi-Fi Timing Parameters:" << std::endl;
    std::cout << "  SlotTime: " << slotTimeUs << " us" << std::endl;
    std::cout << "  SIFS: " << sifsTimeUs << " us" << std::endl;
    std::cout << "  PIFS: " << pifsTimeUs << " us" << std::endl;
    std::cout << "Total received bytes at STA: " << totalRxBytes << std::endl;
    std::cout << "Actual application duration: " << actualSimulationTime << " s" << std::endl;
    std::cout << "Calculated throughput: " << throughputMbps << " Mbps" << std::endl;

    Simulator::Destroy ();

    return 0;
}