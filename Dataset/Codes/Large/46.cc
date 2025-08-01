// This script configures two nodes on an 802.11b physical layer, with
// 802.11b NICs in adhoc mode. One of the nodes generates on-off traffic
// destined to the other node.
//
// The purpose is to test the energy depletion on the nodes and the
// activation of the callback that puts a node in the sleep state when
// its energy is depleted. Furthermore, this script can be used to test
// the available policies for updating the transmit current based on
// the nominal tx power used to transmit each frame.
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./ns3 run "wifi-sleep --help"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the documentation.
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
//
// ./ns3 run "wifi-sleep --verbose=1"
//
// When you are done, you will notice four trace files in your directory:
// two for the remaining energy on each node and two for the state transitions
// of each node.

#include "ns3/basic-energy-source-helper.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/string.h"
#include "ns3/wifi-net-device.h"
#include "ns3/wifi-radio-energy-model-helper.h"
#include "ns3/wifi-utils.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;
using namespace ns3::energy;

NS_LOG_COMPONENT_DEFINE("WifiSleep");

/**
 * Remaining energy trace sink
 *
 * \tparam node The node ID this trace belongs to.
 * \param oldValue Old value.
 * \param newValue New value.
 */
template <int node>
void
RemainingEnergyTrace(double oldValue, double newValue)
{
    std::stringstream ss;
    ss << "energy_" << node << ".log";

    static std::fstream f(ss.str(), std::ios::out);

    f << Simulator::Now().GetSeconds() << "    remaining energy=" << newValue << std::endl;
}

/**
 * PHY state trace sink
 *
 * \tparam node The node ID this trace belongs to.
 * \param context The context
 * \param start Start time for the current state
 * \param duration Duratio of the current state
 * \param state State
 */
template <int node>
void
PhyStateTrace(std::string context, Time start, Time duration, WifiPhyState state)
{
    std::stringstream ss;
    ss << "state_" << node << ".log";

    static std::fstream f(ss.str(), std::ios::out);

    f << Simulator::Now().GetSeconds() << "    state=" << state << " start=" << start
      << " duration=" << duration << std::endl;
}

int
main(int argc, char* argv[])
{
    DataRate dataRate{"1Mb/s"};
    uint32_t packetSize{1000}; // bytes
    Time duration{"10s"};
    joule_u initialEnergy{7.5};
    volt_u voltage{3.0};
    dBm_u txPowerStart{0.0};
    dBm_u txPowerEnd{15.0};
    uint32_t nTxPowerLevels{16};
    uint32_t txPowerLevel{0};
    ampere_u idleCurrent{0.273};
    ampere_u txCurrent{0.380};
    bool verbose{false};

    CommandLine cmd(__FILE__);
    cmd.AddValue("dataRate", "Data rate", dataRate);
    cmd.AddValue("packetSize", "size of application packet sent", packetSize);
    cmd.AddValue("duration", "duration of the experiment", duration);
    cmd.AddValue("initialEnergy", "Initial Energy (Joule) of each node", initialEnergy);
    cmd.AddValue("voltage", "Supply voltage (Joule)", voltage);
    cmd.AddValue("txPowerStart", "Minimum available transmission level (dbm)", txPowerStart);
    cmd.AddValue("txPowerEnd", "Maximum available transmission level (dbm)", txPowerEnd);
    cmd.AddValue("nTxPowerLevels",
                 "Number of transmission power levels available between txPowerStart and "
                 "txPowerEnd included",
                 nTxPowerLevels);
    cmd.AddValue("txPowerLevel", "Transmission power level", txPowerLevel);
    cmd.AddValue("idleCurrent", "The radio Idle current in Ampere", idleCurrent);
    cmd.AddValue("txCurrent", "The radio Tx current in Ampere", txCurrent);
    cmd.AddValue("verbose", "turn on all WifiNetDevice log components", verbose);
    cmd.Parse(argc, argv);

    NodeContainer c;
    c.Create(2);

    // The below set of helpers will help us to put together the wifi NICs we want
    WifiHelper wifi;
    if (verbose)
    {
        WifiHelper::EnableLogComponents(); // Turn on all Wifi logging
    }
    wifi.SetStandard(WIFI_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("TxPowerStart", DoubleValue(txPowerStart));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txPowerEnd));
    wifiPhy.Set("TxPowerLevels", UintegerValue(nTxPowerLevels));

    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Add a mac and set the selected tx power level
    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager",
                                 "DefaultTxPowerLevel",
                                 UintegerValue(txPowerLevel));
    // Set it to adhoc mode
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, c);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(c);

    InternetStackHelper internet;
    internet.Install(c);

    Ipv4AddressHelper ipv4;
    NS_LOG_INFO("Assign IP Addresses.");
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    ApplicationContainer apps;

    std::string transportProto = std::string("ns3::UdpSocketFactory");
    OnOffHelper onOff(transportProto, InetSocketAddress(Ipv4Address("10.1.1.2"), 9000));

    onOff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
    onOff.SetAttribute("PacketSize", UintegerValue(packetSize));
    onOff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.001]"));

    apps = onOff.Install(c.Get(0));

    apps.Start(Seconds(0.01));
    apps.Stop(duration);

    // Create a packet sink to receive these packets
    PacketSinkHelper sink(transportProto, InetSocketAddress(Ipv4Address::GetAny(), 9001));
    apps = sink.Install(c.Get(1));
    apps.Start(Seconds(0.01));
    apps.Stop(duration);

    // Energy sources
    EnergySourceContainer eSources;
    BasicEnergySourceHelper basicSourceHelper;
    WifiRadioEnergyModelHelper radioEnergyHelper;

    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(initialEnergy));
    basicSourceHelper.Set("BasicEnergySupplyVoltageV", DoubleValue(voltage));

    radioEnergyHelper.Set("IdleCurrentA", DoubleValue(idleCurrent));
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(txCurrent));

    // compute the efficiency of the power amplifier (eta) assuming that the provided value for tx
    // current corresponds to the minimum tx power level
    double eta = DbmToW(txPowerStart) / ((txCurrent - idleCurrent) * voltage);

    radioEnergyHelper.SetTxCurrentModel("ns3::LinearWifiTxCurrentModel",
                                        "Voltage",
                                        DoubleValue(voltage),
                                        "IdleCurrent",
                                        DoubleValue(idleCurrent),
                                        "Eta",
                                        DoubleValue(eta));

    // install an energy source on each node
    for (auto n = c.Begin(); n != c.End(); n++)
    {
        eSources.Add(basicSourceHelper.Install(*n));

        Ptr<WifiNetDevice> wnd;

        for (uint32_t i = 0; i < (*n)->GetNDevices(); ++i)
        {
            wnd = (*n)->GetDevice(i)->GetObject<WifiNetDevice>();
            // if it is a WifiNetDevice
            if (wnd)
            {
                // this device draws power from the last created energy source
                radioEnergyHelper.Install(wnd, eSources.Get(eSources.GetN() - 1));
            }
        }
    }

    // Tracing
    eSources.Get(0)->TraceConnectWithoutContext("RemainingEnergy",
                                                MakeCallback(&RemainingEnergyTrace<0>));
    eSources.Get(1)->TraceConnectWithoutContext("RemainingEnergy",
                                                MakeCallback(&RemainingEnergyTrace<1>));

    Config::Connect("/NodeList/0/DeviceList/*/Phy/State/State", MakeCallback(&PhyStateTrace<0>));
    Config::Connect("/NodeList/1/DeviceList/*/Phy/State/State", MakeCallback(&PhyStateTrace<1>));

    Simulator::Stop(duration + Seconds(1.0));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
