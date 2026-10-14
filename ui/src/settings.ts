import { mount } from 'svelte';
import { connect } from './lib/bridge';
import './lib/state.svelte';
import Settings from './settings/Settings.svelte';
import './styles/base.css';

const app = mount(Settings, { target: document.getElementById('app')! });
void connect('settings');

export default app;
